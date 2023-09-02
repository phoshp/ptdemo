//
// Created by Fatih on 7/18/2022.
//

#include "graphics/RenderEngine.hpp"

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <string>
#include <chrono>

namespace ph {

bool RenderEngine::init() {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
		throw std::runtime_error("SDL could not initialized");
	}

	//SDL_DisplayMode display;
	//SDL_GetDesktopDisplayMode(0, &display);
	//m_windowExtent.height = display.h;
	//m_windowExtent.width = display.w;
	
	m_window = SDL_CreateWindow("vulkan raytracer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int) m_windowExtent.width, (int) m_windowExtent.height, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	SDL_SetWindowAlwaysOnTop(m_window, SDL_FALSE);
	if (m_window == nullptr) {
		std::cout << SDL_GetError();
		throw std::runtime_error("SDL window could not created: ");
	}
	//SDL_SetWindowFullscreen(mWindow, SDL_WINDOW_FULLSCREEN);
	SDL_SetRelativeMouseMode(SDL_TRUE);
	auto icon = SDL_LoadBMP("assets/window_icon.bmp");
	if (!icon) {
		std::printf("Failed to load icon: %s\n", SDL_GetError());
	}

	SDL_SetWindowIcon(m_window, icon);
	SDL_FreeSurface(icon);

	/*m_gui = SDL_CreateRGBSurface(0, (int)m_windowExtent.width, (int)m_windowExtent.height, 32, 0, 0, 0, 0);

	if (m_gui == nullptr) {
		throw std::runtime_error(SDL_GetError());
	}*/
	return true;
}

void RenderEngine::tick() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) m_shouldClose = true;

		m_eventHandler(event);

		if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				int width = 0;
				int height = 0;
				SDL_GetWindowSize(m_window, &width, &height);
				while (width == 0 || height == 0) { // background
					SDL_GetWindowSize(m_window, &width, &height);
					SDL_WaitEvent(&event);
				}

				std::cout << "!!! CHANGED SIZE: " << std::to_string(width) << "x" << std::to_string(height) << std::endl;

				m_windowExtent = {(uint32_t) width, (uint32_t) height};
				if (m_renderer != nullptr) m_renderer->resize(m_windowExtent);
			}
		}
	}

	if (m_renderer != nullptr) {
		m_renderer->render();
	}
}

void RenderEngine::updateTitle() {
	// SDL_SetWindowTitle(m_window, ("vulkan raytracer | fps: " + std::to_string(m_renderer->m_frames)).data());
}

void RenderEngine::destroy() {
	if (m_renderer != nullptr) {
		m_renderer->cleanup();
	}
	SDL_DestroyWindow(m_window);
}

} // ph