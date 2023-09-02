//
// Created by Fatih on 8/10/2022.
//

#ifndef PTDEMO_VULKANRENDERER_HPP
#define PTDEMO_VULKANRENDERER_HPP

#include "graphics/Renderer.hpp"
#include "graphics/vulkan/VkBootstrap.h"
#include "graphics/vulkan/VulkanTypes.hpp"

#include <fstream>
#include <iostream>
#include <SDL2/SDL_vulkan.h>
#include <string>
#include <shaderc/shaderc.hpp>

#include <functional>

namespace ph {

class VulkanRenderer : public virtual Renderer {
public:
	VulkanRenderer(SDL_Window* window, vk::Extent2D extent, const std::string& appName);

	void postInitialize() override;

	void resize(VkExtent2D extent) override;

	void render() override;

	void cleanup() override;

	void addBuffer(uint32_t index, size_t size, void *data) override;

	void addUniform(uint32_t index, size_t size, void *data) override;

	void setPushConstants(uint32_t offset, size_t size, void *data) override;

	vkt::PushConstants m_pushConstants;
	std::vector<vkt::StorageData> m_storageDataSet;

private:

	std::vector<uint32_t> compileShader(const std::string& filename);

	void prepareStorageBuffers();

	void createSynchronizationStructs();

	void createSwapchain();

	void recreateSwapchain();

	void createQueue(vkt::Queue* q, const vk::Queue& queue) const;

	void createComputeImage();

	void createComputePipeline(const std::string& shaderFileName);

	std::vector<uint32_t> readShaderFile(const std::string& filename);

	void recordComputeCommands();

	void applyFirstImageBarriers(const vk::CommandBuffer& buffer);

	void copyImageMemory(const vk::CommandBuffer& buffer) const;

	void applySecondImageBarriers(const vk::CommandBuffer& buffer);

	void createStorageBuffer(const void* data, size_t size, vkt::Buffer& buffer, vk::BufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryProperties) const;

	uint32_t m_frameCounter = 0;
	uint64_t m_lastTicks = 0;
	bool m_resized = false;

	SDL_Window* m_window;
	vk::Extent2D m_windowExtent;
	vk::UniqueInstance m_instance;
	vk::UniqueDevice m_device;
	vk::PhysicalDevice m_physicalDevice;
	vk::UniqueSurfaceKHR m_surface;
	VmaAllocator m_allocator{};
	VkDebugUtilsMessengerEXT m_debugMessenger;

	vk::PresentModeKHR m_presentMode = vk::PresentModeKHR::eMailbox;
	vkt::Swapchain m_swapchain;
	vkt::Queue m_presentQueue;
	vkt::Queue m_computeQueue;
	vkt::Image m_computeImage;
	vkt::Pipeline m_computePipeline;
	std::vector<std::function<void(const vk::CommandBuffer&)>> m_computeCommands;

	vkt::Image m_skyBoxImage;

	vk::UniqueFence m_computeFence;
	vk::UniqueSemaphore m_imageAcquiredSemaphore;
	vk::UniqueSemaphore m_renderFinishedSemaphore;

	void createSkybox();
};

} // ph

#endif //PTDEMO_VULKANRENDERER_HPP
