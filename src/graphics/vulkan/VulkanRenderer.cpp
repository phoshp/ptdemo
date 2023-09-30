//
// Created by Fatih on 8/10/2022.
//

#include "graphics/vulkan/VulkanRenderer.hpp"

namespace ph {

VulkanRenderer::VulkanRenderer(SDL_Window* window, const vk::Extent2D extent, const std::string& appName) : m_window(window), m_windowExtent(extent) {
	vkb::InstanceBuilder builder;
	builder.set_app_name(appName.data())
			.request_validation_layers(true)
			.require_api_version(1, 3, 0)
			.use_default_debug_messenger();

	uint32_t extCount;
	SDL_Vulkan_GetInstanceExtensions(window, &extCount, nullptr); // get extension count first
	std::vector<const char*> extensions(extCount); // add additional extensions
	SDL_Vulkan_GetInstanceExtensions(window, &extCount, extensions.data());

	for (auto ext : extensions) {
		builder.enable_extension(ext);
	}
	auto build = builder.build();

	if (!build.has_value()) {
		throw std::runtime_error("Failed to create a vulkan instance (error: " + std::to_string(build.error().value()) + ")");
	}

	m_instance = vk::UniqueInstance(build.value().instance);
	m_debugMessenger = build.value().debug_messenger;

	VkSurfaceKHR surface;
	if (SDL_Vulkan_CreateSurface(m_window, m_instance.get(), &surface) == SDL_FALSE) {
		throw std::runtime_error("Failed to create a vulkan window surface");
	}
	m_surface = vk::UniqueSurfaceKHR(surface, {m_instance.get()});

	vkb::PhysicalDeviceSelector selector(build.value(), m_surface.get());
	vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3).select().value();
	vkb::DeviceBuilder deviceBuilder(physicalDevice);
	vkb::Device vkbDevice = deviceBuilder.build().value();

	m_device = vk::UniqueDevice(vkbDevice.device);
	m_physicalDevice = physicalDevice.physical_device;

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.instance = m_instance.get();
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorInfo.device = m_device.get();
	allocatorInfo.physicalDevice = m_physicalDevice;
	if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a vulkan allocator");
	}

	m_computePipeline.cache = m_device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo());
	m_presentQueue.family = vkbDevice.get_queue_index(vkb::QueueType::present).value();
	m_computeQueue.family = vkbDevice.get_queue_index(vkb::QueueType::compute).value();

	createQueue(&m_presentQueue, m_device->getQueue(m_presentQueue.family, 0));
	createQueue(&m_computeQueue, m_device->getQueue(m_computeQueue.family, 0));
}

void VulkanRenderer::postInitialize() {
	createSwapchain();
	createComputeImage();

	prepareStorageBuffers();
	createComputePipeline("shaders/RTNew.comp");
	createSynchronizationStructs();
}

void VulkanRenderer::resize(const VkExtent2D extent) {
	m_windowExtent = extent;
	m_resized = true;
}

void VulkanRenderer::render() {
	m_pushConstants.update(m_allocator);
	for (const auto& data : m_storageDataSet) {
		data.update(m_allocator);
	}

	m_frameCounter++;
	auto currentTicks = SDL_GetTicks64();
	if (currentTicks - m_lastTicks >= 1000) {
		m_frames = m_frameCounter;
		m_frameCounter = 0;
		m_lastTicks = currentTicks;
	}

	auto result = m_device->waitForFences(m_computeFence.get(), VK_TRUE, UINT64_MAX);
	if (result != vk::Result::eSuccess) {
		vk::detail::throwResultException(result, "Failed to wait for fences");
	}

	auto r2 = m_device->acquireNextImageKHR(m_swapchain.handle.get(), UINT64_MAX, m_imageAcquiredSemaphore.get());
	result = r2.result;
	m_swapchain.currentFrame = r2.value;
	if (result == vk::Result::eErrorOutOfDateKHR) {
		recreateSwapchain();
		return;
	} else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
		vk::detail::throwResultException(result, "Failed to acquire swap chain image!");
	}

	recordComputeCommands();
	m_device->resetFences(m_computeFence.get());

	vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eTopOfPipe;
	vk::SubmitInfo submitInfo(m_imageAcquiredSemaphore.get(), waitStages, m_computeQueue.buffers, m_renderFinishedSemaphore.get());
	result = m_computeQueue.handle.submit(1, &submitInfo, m_computeFence.get());
	if (result != vk::Result::eSuccess)
		vk::detail::throwResultException(result, "Failed to submit Compute Command Buffers to Compute Queue !");

	vk::PresentInfoKHR presentInfo(m_renderFinishedSemaphore.get(), m_swapchain.handle.get(), m_swapchain.currentFrame);
	result = m_presentQueue.handle.presentKHR(&presentInfo);

	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_resized) {
		m_resized = false;
		recreateSwapchain();
	} else if (result != vk::Result::eSuccess)
		vk::detail::throwResultException(result, "Failed to present swap chain image !");
}

void VulkanRenderer::cleanup() {
	m_device->waitIdle();

	vmaDestroyImage(m_allocator, m_computeImage.handle, m_computeImage.alloc);
	for (auto& data : m_storageDataSet) {
		vmaDestroyBuffer(m_allocator, data.buffer.handle, data.buffer.alloc);
	}
	if (m_pushConstants.data != nullptr) {
		vmaDestroyBuffer(m_allocator, m_pushConstants.buffer.handle, m_pushConstants.buffer.alloc);
	}
	vmaDestroyAllocator(m_allocator);
}

void VulkanRenderer::addBuffer(uint32_t index, size_t size, void* data) {
	m_storageDataSet.push_back(vkt::StorageData{
			{},
			vkt::ShaderBinding{index, vk::DescriptorType::eStorageBuffer},
			vk::BufferUsageFlagBits::eStorageBuffer,
			size,
			data
	});
}

void VulkanRenderer::addUniform(uint32_t index, size_t size, void* data) {
	m_storageDataSet.push_back(vkt::StorageData{
			{},
			vkt::ShaderBinding{index, vk::DescriptorType::eUniformBuffer},
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer,
			size,
			data
	});
}

void VulkanRenderer::setPushConstants(uint32_t offset, size_t size, void* data) {
	m_pushConstants.offset = offset;
	m_pushConstants.size = size;
	m_pushConstants.data = data;
}

std::vector<uint32_t> VulkanRenderer::compileShader(const std::string& filename) {
	std::ifstream file(filename);

	if (!file.is_open())
		throw std::runtime_error("Failed to open file: " + filename);

	std::stringstream buffer;
	buffer << file.rdbuf();
	file.close();

	shaderc::Compiler compiler;
	auto result = compiler.CompileGlslToSpv(buffer.str(), shaderc_shader_kind::shaderc_compute_shader, filename.data());
	// success
	if (result.GetCompilationStatus() == 0) {
		return { result.begin(), result.end() };
	} else {
		throw std::runtime_error("Failed to compile shader " + filename + ": " + result.GetErrorMessage());
	}
}

void VulkanRenderer::prepareStorageBuffers() {
	if (m_pushConstants.data != nullptr) {
		createStorageBuffer(m_pushConstants.data, m_pushConstants.size, m_pushConstants.buffer, vk::BufferUsageFlagBits::eStorageBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	for (auto& data: m_storageDataSet) {
		createStorageBuffer(data.data, data.size, data.buffer, data.usageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}
}

void VulkanRenderer::createSynchronizationStructs() {
	m_computeFence = m_device->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
	m_imageAcquiredSemaphore = m_device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
	m_renderFinishedSemaphore = m_device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
}

void VulkanRenderer::createSwapchain() {
	vkb::SwapchainBuilder builder{m_physicalDevice, m_device.get(), m_surface.get()};

	auto build = builder
			.set_desired_format({VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
			.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
			.set_desired_extent(m_windowExtent.width, m_windowExtent.height)
			.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			.set_old_swapchain(m_swapchain.handle.get())
			.build();
	vkb::Swapchain value;

	if (build.has_value()) {
		value = build.value();
	} else {
		throw std::runtime_error(build.error().message());
	}

	m_swapchain.handle = vk::UniqueSwapchainKHR(value.swapchain, {m_device.get()});
	m_swapchain.imageFormat = vk::Format(value.image_format);
	std::cout << vk::to_string(m_swapchain.imageFormat) << std::endl;

	m_swapchain.extent = value.extent;
	m_swapchain.dispatchSize = { ceil(m_swapchain.extent.width / 8), ceil(m_swapchain.extent.height / 8), 1 };

	const auto imgs = value.get_images().value();
	const auto views = value.get_image_views().value();

	for (auto& img : imgs) {
		m_swapchain.images.emplace_back(img);
	}
	for (auto& view : views) {
		m_swapchain.imageViews.push_back(vk::UniqueImageView(view, {m_device.get()}));
	}
}

void VulkanRenderer::recreateSwapchain() {
	m_device->waitIdle();

	m_swapchain.imageViews.clear();
	m_swapchain.images.clear();

	m_computePipeline.descriptorSets.clear();
	m_computePipeline.descriptorPool.reset();
	m_computePipeline.layout.reset();
	m_computePipeline.handle.reset();
	m_computeImage.views.clear();
	vmaDestroyImage(m_allocator, m_computeImage.handle, m_computeImage.alloc);
	m_swapchain.handle.reset();

	createSwapchain();
	createComputeImage();
	createComputePipeline("shaders/RTNew.comp");
	createSynchronizationStructs();
}

void VulkanRenderer::createQueue(vkt::Queue* const q, const vk::Queue& queue) const {
	q->handle = queue;
	q->commandPool = m_device->createCommandPoolUnique(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, q->family));
	auto buffers = m_device->allocateCommandBuffers(vk::CommandBufferAllocateInfo(q->commandPool.get(), vk::CommandBufferLevel::ePrimary, 1));
	for (auto& buf : buffers) {
		q->buffers.push_back(buf);
	}
}

void VulkanRenderer::createComputeImage() {
	VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = (VkFormat)m_swapchain.imageFormat;
	info.extent = {m_swapchain.extent.width, m_swapchain.extent.height, 1};
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VkImage image;
	auto result = vmaCreateImage(m_allocator, &info, &allocInfo, &image, &m_computeImage.alloc, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create compute image ! (error code " + std::to_string(result) + ")");

	m_computeImage.handle = image;

	vk::ImageViewCreateInfo viewInfo(
			{},
			m_computeImage.handle,
			vk::ImageViewType::e2D,
			m_swapchain.imageFormat,
			vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA),
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
			);

	m_computeImage.views.push_back(m_device->createImageViewUnique(viewInfo));
	m_computeImage.barrier.init(m_computeImage.handle, vk::ImageLayout::eUndefined, vk::AccessFlagBits::eNone, m_computeQueue.family);
}

void VulkanRenderer::createComputePipeline(const std::string& shaderFileName) {
	auto imageInfo = vk::DescriptorImageInfo({}, m_computeImage.views[0].get(), vk::ImageLayout::eGeneral);
	vkt::DescriptorPoolBuilder builder;
	auto& set = builder.set().bindImage({0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute}, imageInfo);
	for (auto& data: m_storageDataSet) {
		set.bindBuffer({data.binding.index, data.binding.descriptorType, 1, data.binding.stageFlags}, data.buffer.info);
	}
	set.build(m_device.get(), {});
	std::vector<vk::DescriptorSetLayout> layouts;
	std::vector<vk::PushConstantRange> constantRanges;
	layouts.push_back(set.layout);
	if (m_pushConstants.data != nullptr) {
		constantRanges.emplace_back(m_pushConstants.stageFlags, m_pushConstants.offset, m_pushConstants.size);
	}

	builder.build(m_device.get(), m_computePipeline, 5);
	m_computePipeline.layout = m_device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo({}, layouts, constantRanges));

	// create pipeline
	auto shaderData = compileShader(shaderFileName);
	auto module = m_device->createShaderModuleUnique(vk::ShaderModuleCreateInfo({}, shaderData));
	vk::PipelineShaderStageCreateInfo stageInfo({}, vk::ShaderStageFlagBits::eCompute, module.get(), "main");

	vk::ComputePipelineCreateInfo pipelineInfo({}, stageInfo, m_computePipeline.layout.get());

	m_computePipeline.handle = m_device->createComputePipelineUnique(m_computePipeline.cache.get(), pipelineInfo).value;
}

std::vector<uint32_t> VulkanRenderer::readShaderFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("Failed to open binary file: " + filename);

	auto fileSize = file.tellg();
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);
	file.close();

	return buffer;
}

void VulkanRenderer::recordComputeCommands() {
	auto& buffer = m_computeQueue.buffers[0];
	buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));

	for (auto& func : m_computeCommands) {
		func(buffer);
	}

	// set an image memory barrier for each image separately.
	applyFirstImageBarriers(buffer);
	copyImageMemory(buffer);
	applySecondImageBarriers(buffer);

	// Bind current descriptor set for each image in the swap chain.
	buffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_computePipeline.handle.get());
	buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_computePipeline.layout.get(), 0, m_computePipeline.descriptorSets, {});
	if (m_pushConstants.data != nullptr) {
		buffer.pushConstants(m_computePipeline.layout.get(), m_pushConstants.stageFlags, m_pushConstants.offset, m_pushConstants.size, m_pushConstants.data);
	}
	buffer.dispatch(m_swapchain.dispatchSize.x, m_swapchain.dispatchSize.y, m_swapchain.dispatchSize.z);
	buffer.end();
}

void VulkanRenderer::applyFirstImageBarriers(const vk::CommandBuffer& buffer) {
	const vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
	vkt::ImageMemoryBarrier swapTransfer(m_swapchain.images[m_swapchain.currentFrame], vk::ImageLayout::eUndefined, vk::AccessFlagBits::eMemoryRead, m_presentQueue.family);

	buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, {
		m_computeImage.barrier.range(subresourceRange).access(vk::AccessFlagBits::eTransferRead).layout(vk::ImageLayout::eTransferSrcOptimal).handle,
		swapTransfer.range(subresourceRange).access(vk::AccessFlagBits::eTransferWrite).layout(vk::ImageLayout::eTransferDstOptimal).handle
	});
}

void VulkanRenderer::copyImageMemory(const vk::CommandBuffer& buffer) const {
	const vk::ImageSubresourceLayers layers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
	vk::ImageCopy copy(layers, {0, 0, 0}, layers, {0, 0, 0}, {m_swapchain.extent.width, m_swapchain.extent.height, 1});

	buffer.copyImage(m_computeImage.handle, vk::ImageLayout::eTransferSrcOptimal, m_swapchain.images[m_swapchain.currentFrame], vk::ImageLayout::eTransferDstOptimal, copy);
}

void VulkanRenderer::applySecondImageBarriers(const vk::CommandBuffer& buffer) {
	const vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
	vkt::ImageMemoryBarrier swapPresent(m_swapchain.images[m_swapchain.currentFrame], vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite, m_presentQueue.family);

	buffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, {
		m_computeImage.barrier.range(subresourceRange).access(vk::AccessFlagBits::eNone).layout(vk::ImageLayout::eGeneral).handle,
		swapPresent.range(subresourceRange).access(vk::AccessFlagBits::eMemoryRead).layout(vk::ImageLayout::ePresentSrcKHR).handle
	});
}

void VulkanRenderer::createStorageBuffer(const void* data, size_t size, vkt::Buffer& buffer, vk::BufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryProperties) const {
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.usage = (VkBufferUsageFlags) usageFlags | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.requiredFlags = memoryProperties;

	VkBuffer buf;
	auto result = vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &buf, &buffer.alloc, nullptr);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Failed to create storage buffer !");

	buffer.handle = buf;
	buffer.info = vk::DescriptorBufferInfo(buffer.handle, 0, size);
	buffer.copyMemory(m_allocator, data, size);
}

} // ph
