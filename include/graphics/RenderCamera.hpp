//
// Created by Fatih on 8/15/2022.
//

#ifndef PTDEMO_RENDERCAMERA_HPP
#define PTDEMO_RENDERCAMERA_HPP

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

namespace ph {

class RenderCamera {
public:

	explicit RenderCamera(glm::dvec4 pos, float aspectRatio, float focalDist) : m_position(pos), m_aspectRatio(aspectRatio), m_focalDistance(focalDist) {
		m_up = {0, 1, 0, 1};
		m_oldPosition = pos;
	}

	void lookAt(glm::vec3 pos) {
		m_oldForward = m_forward;
		// m_forward = { glm::normalize(pos - m_position.xyz), 1 };
	}

	void roll(float value) {
		auto right = glm::cross(m_forward.xyz(), m_up.xyz());
		m_up = {glm::normalize(m_up.xyz() + right * value), 1};
	}

	void updateDirection(float yaw, float pitch) {
		auto yawRad = glm::radians(yaw);
		auto pitchRad = glm::radians(pitch);

		m_oldForward = m_forward;
		m_forward = {
				cos(yawRad) * cos(pitchRad),
				sin(pitchRad),
				sin(yawRad) * cos(pitchRad),
				1
		};
	}

	void updatePosition(const glm::vec3& pos) {
		m_oldPosition = m_position;
		m_position = {pos, 1};
	}

	void move(const glm::vec3& motion) {
		updatePosition(m_position.xyz() + motion);
	}

	glm::vec4 m_position;
	glm::vec4 m_oldPosition{};
    glm::vec4 m_forward{};
	glm::vec4 m_oldForward{};
	glm::vec4 m_up{};

	float m_aspectRatio;
	float m_focalDistance;
	int m_samples = 8;
	float m_time = 0.0;
};

} // ph

#endif //PTDEMO_RENDERCAMERA_HPP
