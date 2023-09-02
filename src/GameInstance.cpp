//
// Created by Fatih on 8/16/2022.
//

#include "GameInstance.hpp"

namespace ph {

void GameInstance::init() {
	m_engine.init();
	auto renderer = new VulkanRenderer(m_engine.m_window, m_engine.m_windowExtent, "PT Demo");
	m_engine.m_renderer = renderer;
	m_engine.m_eventHandler = [this] (const auto& e) { handleEvent(std::move(e)); };

	Material mat1{
		.albedo = {0.0, 0.0, 0.0},
		.metallic = 0.0,
		.roughness = 0.9,
		.specular = 0.0,
		.specTrans = 0.0,
		.ior = 0
	};
	Material mat2{
		.albedo = {0.2, 0.2, 0.2},
		.metallic = 0.1,
		.roughness = 0.75,
		.specular = 0.0,
		.specTrans = 0.0,
		.ior = 0
	};
	Material mat3{
		.albedo = {0.2, 0.2, 0.2},
		.metallic = 0.6,
		.roughness = 0.8,
		.specular = 0.0,
		.specTrans = 0.0,
		.ior = 0
	};

	spheres.push_back(Sphere{{-0.55, 1.55, -8.0}, 0, {0, 0, 1.0}, 1.0, mat2});
	spheres.push_back(Sphere{{1.3, 1.2, 40.2}, 0, {1.0, 1.0, 1.0}, 0.8, mat1});
	planes.push_back(Plane{{0, -2, 0}, 0, {0, 1, 0}, 0, {0.3, 0.3, 0.3}, 0, mat3});

	boxes.push_back(Box({10, -1, -1}, {3, 5, 3}, {0.3, 0.4, 255}, mat1));
	randomizeSpheres();

	//spotLights.push_back(SpotLight{{0, 1, -3.2}, 4, glm::vec3(1.0), 2.0});
	spotLights.push_back(SpotLight{{-4, 40, -3.2}, 4, glm::vec3(1.0), 2.0});
	directLights.push_back(DirectLight{{0, 1, 0}, 0, {1, 1, 1}, 0});

	m_camera = RenderCamera({0, 2, 5, 1}, 1.7, 2.2);
	m_camera.lookAt({0, 0, -1});
	m_yaw = -90;

	/*stbi_set_flip_vertically_on_load(true);
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load("assets/panorama.exr", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);*/


	renderer->setPushConstants(0, sizeof(RenderCamera), &m_camera);
	renderer->addBuffer(1, sizeof(Sphere) * spheres.size(), spheres.data());
	renderer->addBuffer(2, sizeof(Plane) * planes.size(), planes.data());
	renderer->addBuffer(3, sizeof(Box) * boxes.size(), boxes.data());
	renderer->addBuffer(4, sizeof(SpotLight) * spotLights.size(), spotLights.data());
	renderer->addBuffer(5, sizeof(DirectLight) * directLights.size(), directLights.data());

	renderer->postInitialize();
}

void GameInstance::randomizeSpheres() {
	// random sphere generation
	auto dist = std::uniform_int_distribution<int>(10, 15);
	auto distXZ = std::uniform_real_distribution<float>(-10.0, 10.0);
	auto distY = std::uniform_real_distribution<float>(0.0, 2.0);
	auto distMat = std::uniform_real_distribution<float>(0, 1.0);
	int maxSp = dist(rnd);

	for (int i = 0; i < 16; ++i) {
		float spec = distMat(rnd) * 0.8;
		float refI = 1.0;
		if (spec < 0.1) {
			refI = 1 + spec;
			spec = 0.0;
		}
		Material mat = {
			.albedo = glm::vec3{0.2, 0.2, 0.2},
			.metallic = distMat(rnd),
			.roughness = distMat(rnd),
			.specular = spec,
			.specTrans = 1.0,
			.ior = refI
		};
		spheres.push_back({ {distXZ(rnd), distY(rnd), distXZ(rnd)}, 0, {distMat(rnd), distMat(rnd), distMat(rnd)}, distMat(rnd) + 0.3f, mat});
	}
}

int maxFPS = 300;

void GameInstance::run() {
	using namespace std::chrono;

	m_running = true;

	auto lastTime = steady_clock::now();
	auto lastTimeEngine = lastTime;
	auto frameTime = 1000 / maxFPS;

	while (m_running && !m_engine.m_shouldClose) {
		auto now = steady_clock::now();
		auto diff = duration_cast<milliseconds>(now - lastTimeEngine).count();
		if (diff >= frameTime) {
			m_engine.tick();
			lastTimeEngine = now;
		}
		
		diff = duration_cast<milliseconds>(now - lastTime).count();
		if (diff >= 10) {
			tickGame(diff * 0.01);
			m_engine.updateTitle();
			lastTime = now;
		}
	}

	m_engine.destroy();
}

float c = 0;

void GameInstance::tickGame(float dt) {
	const uint8_t* keyState = SDL_GetKeyboardState(nullptr);
	if (keyState[SDL_SCANCODE_W]) m_input.moveForward();
	if (keyState[SDL_SCANCODE_S]) m_input.moveBackward();
	if (keyState[SDL_SCANCODE_A]) m_input.moveLeft();
	if (keyState[SDL_SCANCODE_D]) m_input.moveRight();
	if (keyState[SDL_SCANCODE_LSHIFT]) m_input.moveDown();
	if (keyState[SDL_SCANCODE_SPACE]) m_input.moveUp();
	if (keyState[SDL_SCANCODE_RIGHT]) m_camera.roll(0.01);
	if (keyState[SDL_SCANCODE_LEFT]) m_camera.roll(-0.01);
	if (keyState[SDL_SCANCODE_E]) m_camera.m_samples += 1;
	if (keyState[SDL_SCANCODE_Q]) m_camera.m_samples -= 1;
	if (keyState[SDL_SCANCODE_R]) {
		auto first = spheres[0];
		spheres.clear();
		spheres.push_back(first);
		randomizeSpheres();
	}

	auto mouseState = SDL_GetMouseState(nullptr, nullptr);
	if ((mouseState & SDL_BUTTON_RMASK) != 0) {
		spotLights[0].position += m_input.motion;
		m_input.motion = {};
	}

	// update aspect ratio when window size changed
	m_camera.m_aspectRatio = float(m_engine.m_windowExtent.width) / m_engine.m_windowExtent.height;
	m_camera.m_time += 0.01;
	if (m_camera.m_time > 1.0) {
		m_camera.m_time = 0.0;
	}
	m_input.update(dt);
}

void GameInstance::handleEvent(const SDL_Event& event) {
	if (event.type == SDL_MOUSEMOTION) {
		m_yaw += float(event.motion.xrel) * m_mouseSens;
		m_pitch += float(-event.motion.yrel) * m_mouseSens;
		if (m_pitch > 89.0f) m_pitch = 89.0f;
		if (m_pitch < -89.0f) m_pitch = -89.0f;

		m_camera.updateDirection(m_yaw, m_pitch);
	} else if (event.type == SDL_MOUSEWHEEL) {
		spotLights[0].intensity += event.wheel.preciseY * 2;
	}
}

} // ph
