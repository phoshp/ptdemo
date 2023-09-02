//
// Created by Fatih on 8/15/2022.
//

#ifndef PTDEMO_VULKANTYPES_HPP
#define PTDEMO_VULKANTYPES_HPP

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <ranges>
#include <unordered_map>
//#include <stb_image.h>

namespace ph::vkt {

struct Queue {
	vk::Queue handle;
	uint32_t family;
	vk::UniqueCommandPool commandPool;
	std::vector<vk::CommandBuffer> buffers;
};

struct Swapchain {
	vk::UniqueSwapchainKHR handle;
	vk::Format imageFormat;
	vk::Extent2D extent;
	glm::ivec3 dispatchSize;
	uint32_t currentFrame = 0;
	std::vector<vk::Image> images;
	std::vector<vk::UniqueImageView> imageViews;
};

struct Pipeline {
	vk::UniquePipeline handle;
	vk::UniquePipelineLayout layout;
	vk::UniquePipelineCache cache;
	vk::UniqueDescriptorPool descriptorPool;
	std::vector<vk::DescriptorSet> descriptorSets;
	std::vector<vk::UniqueDescriptorSetLayout> descriptorSetLayouts;
};

struct DescriptorSetBuilder {
public:
	DescriptorSetBuilder() = default;

	DescriptorSetBuilder& bind(vk::DescriptorSetLayoutBinding binding) {
		bindings.push_back(binding);
		return *this;
	}

	DescriptorSetBuilder& bindBuffer(vk::DescriptorSetLayoutBinding binding, vk::DescriptorBufferInfo& info) {
		bind(binding);
		bufferInfos[binding.binding] = {info};
		return *this;
	}

	DescriptorSetBuilder& bindBufferArray(vk::DescriptorSetLayoutBinding binding, std::initializer_list<vk::DescriptorBufferInfo>& infos) {
		bind(binding);
		bufferInfos[binding.binding] = infos;
		return *this;
	}

	DescriptorSetBuilder& bindImage(vk::DescriptorSetLayoutBinding binding, vk::DescriptorImageInfo& info) {
		bind(binding);
		imageInfos[binding.binding] = {info};
		return *this;
	}

	DescriptorSetBuilder& bindImageArray(vk::DescriptorSetLayoutBinding binding, std::initializer_list<vk::DescriptorImageInfo>& infos) {
		bind(binding);
		imageInfos[binding.binding] = infos;
		return *this;
	}

	const vk::DescriptorSetLayout& build(const vk::Device& device, vk::DescriptorSetLayoutCreateFlags flags) {
		layout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo(flags, bindings));
		return layout;
	}

	std::vector<vk::DescriptorSetLayoutBinding> bindings;
	vk::DescriptorSetLayout layout;
	std::unordered_map<uint32_t, std::vector<vk::DescriptorBufferInfo>> bufferInfos;
	std::unordered_map<uint32_t, std::vector<vk::DescriptorImageInfo>> imageInfos;
};

struct DescriptorPoolBuilder {
public:
	DescriptorPoolBuilder() = default;

	DescriptorSetBuilder& set() {
		sets.emplace_back();
		return sets.back();
	}

	DescriptorPoolBuilder& flags(vk::DescriptorPoolCreateFlags flags) {
		createFlags = flags;
		return *this;
	}

	void build(const vk::Device& device, vkt::Pipeline& pipeline, uint32_t maxSets = 0) {
		std::unordered_map<vk::DescriptorType, vk::DescriptorPoolSize> poolSizes;

		for (auto& set: sets) {
			for (auto& binding: set.bindings) {
				if (poolSizes.contains(binding.descriptorType)) {
					poolSizes[binding.descriptorType].descriptorCount += binding.descriptorCount;
				} else {
					poolSizes[binding.descriptorType] = vk::DescriptorPoolSize(binding.descriptorType, binding.descriptorCount);
				}
			}
		}

		std::vector<vk::DescriptorPoolSize> sizes;
		std::vector<vk::DescriptorSetLayout> layouts;
		std::vector<vk::WriteDescriptorSet> writeSets;

		sizes.resize(poolSizes.size());
		layouts.resize(sets.size());
		std::transform(poolSizes.begin(), poolSizes.end(), sizes.begin(), [](auto kv) { return kv.second; });
		std::transform(sets.begin(), sets.end(), layouts.begin(), [](auto s) { return s.layout; });

		pipeline.descriptorPool = device.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo(createFlags, (maxSets > 0 ? maxSets : sets.size()), sizes));

		pipeline.descriptorSets.resize(sets.size());
		pipeline.descriptorSetLayouts.resize(sets.size());

		auto allocated = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(pipeline.descriptorPool.get(), layouts));
		for (int i = 0; i < sets.size(); ++i) {
			pipeline.descriptorSets[i] = allocated[i];
			pipeline.descriptorSetLayouts[i] = vk::UniqueDescriptorSetLayout(layouts[i], device);

			auto& set = sets[i];
			for (auto& binding: set.bindings) {
				if (set.bufferInfos.contains(binding.binding)) {
					writeSets.push_back(vk::WriteDescriptorSet(pipeline.descriptorSets[i], binding.binding, 0, binding.descriptorType, {}, set.bufferInfos[binding.binding]));
				} else if (set.imageInfos.contains(binding.binding)) {
					writeSets.emplace_back(pipeline.descriptorSets[i], binding.binding, 0, binding.descriptorType, set.imageInfos[binding.binding]);
				} else {
					writeSets.push_back(vk::WriteDescriptorSet(pipeline.descriptorSets[i], binding.binding, 0, binding.descriptorType, {}, set.bufferInfos[binding.binding]));
				}
			}
		}

		device.updateDescriptorSets(writeSets, {});
	}

private:
	std::vector<DescriptorSetBuilder> sets;
	vk::DescriptorPoolCreateFlags createFlags;
};

struct ImageMemoryBarrier {

	ImageMemoryBarrier() = default;

	explicit ImageMemoryBarrier(const vk::Image& image, vk::ImageLayout initLayout, vk::AccessFlags initAccess, uint32_t initFamily = VK_QUEUE_FAMILY_IGNORED) {
		init(image, initLayout, initAccess, initFamily);
	}

	inline void init(const vk::Image& image, vk::ImageLayout initLayout, vk::AccessFlags initAccess, uint32_t initFamily = VK_QUEUE_FAMILY_IGNORED) {
		handle.setImage(image);
		handle.setOldLayout(initLayout);
		handle.setNewLayout(initLayout);
		handle.setSrcAccessMask(initAccess);
		handle.setDstAccessMask(initAccess);
		handle.setSrcQueueFamilyIndex(initFamily);
		handle.setDstQueueFamilyIndex(initFamily);
	}

	inline ImageMemoryBarrier& range(vk::ImageSubresourceRange range) {
		handle.subresourceRange = range;
		return *this;
	}

	inline ImageMemoryBarrier& layout(vk::ImageLayout layout) {
		handle.oldLayout = handle.newLayout;
		handle.newLayout = layout;
		return *this;
	}

	inline ImageMemoryBarrier& access(vk::AccessFlags access) {
		handle.srcAccessMask = handle.dstAccessMask;
		handle.dstAccessMask = access;
		return *this;
	}

	inline ImageMemoryBarrier& family(uint32_t index) {
		handle.srcQueueFamilyIndex = handle.dstQueueFamilyIndex;
		handle.dstQueueFamilyIndex = index;
		return *this;
	}

	inline void apply(const vk::CommandBuffer& buffer, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {
		buffer.pipelineBarrier(srcStage, dstStage, {}, {}, {}, {handle});
	}

	vk::ImageMemoryBarrier handle;
};

struct BufferMemoryBarrier {

	BufferMemoryBarrier() = default;

	explicit BufferMemoryBarrier(const vk::Buffer& buffer, vk::AccessFlags initAccess, VkDeviceSize offset, VkDeviceSize size, uint32_t initFamily = VK_QUEUE_FAMILY_IGNORED) {
		init(buffer, initAccess, offset, size, initFamily);
	}

	inline void init(const vk::Buffer& buffer, vk::AccessFlags initAccess, VkDeviceSize offset, VkDeviceSize size, uint32_t initFamily = VK_QUEUE_FAMILY_IGNORED) {
		handle.setBuffer(buffer);
		handle.setSrcAccessMask(initAccess);
		handle.setDstAccessMask(initAccess);
		handle.setSrcQueueFamilyIndex(initFamily);
		handle.setDstQueueFamilyIndex(initFamily);
		handle.setOffset(offset);
		handle.setSize(size);
	}

	inline BufferMemoryBarrier& offset(VkDeviceSize offset) {
		handle.offset = offset;
		return *this;
	}

	inline BufferMemoryBarrier& size(VkDeviceSize size) {
		handle.size = size;
		return *this;
	}

	inline BufferMemoryBarrier& access(vk::AccessFlags access) {
		handle.srcAccessMask = handle.dstAccessMask;
		handle.dstAccessMask = access;
		return *this;
	}

	inline BufferMemoryBarrier& family(uint32_t index) {
		handle.srcQueueFamilyIndex = handle.dstQueueFamilyIndex;
		handle.dstQueueFamilyIndex = index;
		return *this;
	}

	inline void apply(const vk::CommandBuffer& buffer, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {
		buffer.pipelineBarrier(srcStage, dstStage, {}, {}, {handle}, {});
	}

	vk::BufferMemoryBarrier handle;
};

struct MemoryBarrier {

	MemoryBarrier() = default;

	explicit MemoryBarrier(vk::AccessFlags initAccess) {
		init(initAccess);
	}

	inline void init(vk::AccessFlags initAccess) {
		handle.setSrcAccessMask(initAccess);
		handle.setDstAccessMask(initAccess);
	}

	inline MemoryBarrier& access(vk::AccessFlags access) {
		handle.srcAccessMask = handle.dstAccessMask;
		handle.dstAccessMask = access;
		return *this;
	}

	inline void apply(const vk::CommandBuffer& buffer, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {
		buffer.pipelineBarrier(srcStage, dstStage, {}, {handle}, {}, {});
	}

	vk::MemoryBarrier handle;
};

struct Image {
	vk::Image handle;
	VmaAllocation alloc;
	std::vector<vk::UniqueImageView> views;
	ImageMemoryBarrier barrier;

	/*void loadFromFile(const std::string& file) {
		stbi_set_flip_vertically_on_load(true);
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("assets/panorama.exr", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	}*/
};

struct Buffer {
	vk::Buffer handle;
	vk::DescriptorBufferInfo info;
	VmaAllocation alloc;

	void copyMemory(const VmaAllocator& allocator, const void* data, size_t size) const {
		void* mapped = nullptr;

		auto result = vmaMapMemory(allocator, alloc, &mapped);
		if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to map memory for buffer !");

		std::memcpy(mapped, data, size);

		if (mapped) {
			vmaUnmapMemory(allocator, alloc);
			mapped = nullptr;
		}
	}
};

struct ShaderBinding {
	uint32_t index{};
	vk::DescriptorType descriptorType{};
	vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eCompute;
};

struct PushConstants {
	Buffer buffer;

	uint32_t offset{};
	uint32_t size{};
	void* data{};

	vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eCompute;

	void update(const VmaAllocator& allocator) const {
		if (data != nullptr) buffer.copyMemory(allocator, data, size);
	}
};

struct StorageData {
	Buffer buffer;
	ShaderBinding binding;
	vk::BufferUsageFlags usageFlags;

	size_t size{};
	void* data{};

	void update(const VmaAllocator& allocator) const {
		if (data != nullptr) buffer.copyMemory(allocator, data, size);
	}
};

}

#endif //PTDEMO_VULKANTYPES_HPP
