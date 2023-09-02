//
// Created by Fatih on 7/17/2022.
//

#ifndef PTDEMO_RENDERER_HPP
#define PTDEMO_RENDERER_HPP

#include <SDL2/SDL.h>

struct VkExtent2D;

namespace ph {

class Renderer {
public:

	virtual void postInitialize() = 0;

	virtual void render() = 0;

	virtual void resize(VkExtent2D extent) = 0;

	virtual void cleanup() = 0;

	virtual void addBuffer(uint32_t index, size_t size, void* data) = 0;

	virtual void addUniform(uint32_t index, size_t size, void* data) = 0;

	virtual void setPushConstants(uint32_t offset, size_t size, void* data) = 0;

	uint32_t m_frames = 0;
};

} // ph

#endif //PTDEMO_RENDERER_HPP
