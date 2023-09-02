//
// Created by Fatih on 8/16/2022.
//

#ifndef PTDEMO_GAMEINSTANCE_HPP
#define PTDEMO_GAMEINSTANCE_HPP

#include <vector>
#include "graphics/RenderCamera.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/RenderEngine.hpp"

#include <chrono>
#include <random>

namespace ph {

struct Material {
	glm::vec3 albedo;
	float metallic;
    float roughness;
    float specular;
    float specTrans;
    float ior;
};

struct Plane {
	glm::vec3 position;
	float pad0;
	glm::vec3 normal;
	float pad1;
	glm::vec3 color;
	float pad2;
	Material mat;
};

struct Sphere {
	glm::vec3 position;
	float pad0;
	glm::vec3 color;
	float radius;
	Material mat;
};

struct Box {
	Box(const glm::vec3 pos, const glm::vec3 size, const glm::vec3 color, const Material& mat) : min(pos), max(pos + size), color(color), mat(mat) {}

	glm::vec3 min;
	float pad0;
	glm::vec3 max;
	float pad1;
	glm::vec3 color;
	float pad2;
	Material mat;
};

struct DirectLight {
    glm::vec3 direction;
    float intensity;
    glm::vec3 color;
    float pad0;
};

struct SpotLight {
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    float radius;
};

struct MovementInput {
	RenderCamera& camera;

	float airDrag = 0.5;
	float movementSpeed = 0.2;
	glm::vec3 motion{};

	explicit MovementInput(RenderCamera& camera) : camera(camera) {}

	void update(float dt) {
		motion -= motion * airDrag * dt;
		if (glm::length(motion) < 0.001) {
			motion = glm::vec4(0);
		}
		camera.move(motion * dt);
	}

	void moveUp() {
		motion.y += movementSpeed;
	}

	void moveDown() {
		motion.y -= movementSpeed;
	}

	void moveRight() {
		auto right = glm::cross(camera.m_forward.xyz(), camera.m_up.xyz());
		motion += right * movementSpeed;
	}

	void moveLeft() {
		auto right = glm::cross(camera.m_forward.xyz(), camera.m_up.xyz());
		motion -= right * movementSpeed;
	}

	void moveForward() {
		motion += camera.m_forward.xyz() * movementSpeed;
	}

	void moveBackward() {
		motion -= camera.m_forward.xyz() * movementSpeed;
	}
};

class GameInstance {
public:

	void init();

	void randomizeSpheres();

	void run();

	void tickGame(float dt);

	void handleEvent(const SDL_Event& event);

	void shutdown() { m_running = false; }

private:

	RenderEngine m_engine;
	bool m_running = false;

	RenderCamera m_camera{{}, 1, 1};
	MovementInput m_input{m_camera};
	float m_mouseSens = 0.2;
	float m_yaw = 0, m_pitch = 0;

	std::vector<Sphere> spheres;
	std::vector<Plane> planes;
	std::vector<Box> boxes;
	std::vector<SpotLight> spotLights;
	std::vector<DirectLight> directLights;

	std::default_random_engine rnd;

};

} // ph

#endif //PTDEMO_GAMEINSTANCE_HPP
