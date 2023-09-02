//
// Created by Fatih on 7/18/2022.
//

#ifndef PTDEMO_RENDERENGINE_HPP
#define PTDEMO_RENDERENGINE_HPP

#include "graphics/vulkan/VulkanRenderer.hpp"

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <functional>

namespace ph {

class RenderEngine {
public:

	bool init();

	void tick();

	void close() { m_shouldClose = true; }

	void updateTitle();

	void destroy();

	SDL_Window* m_window = nullptr;
	VkExtent2D m_windowExtent {1080, 720};
	Renderer* m_renderer = nullptr;
	std::function<void(const SDL_Event&)> m_eventHandler;

	bool m_shouldClose = false;
};

} // ph

#endif //PTDEMO_RENDERENGINE_HPP
