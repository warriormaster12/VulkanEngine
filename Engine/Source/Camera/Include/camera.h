// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"
#include "vk_swapchain.h"
#include "window_handler.h"

#include <glm/glm.hpp>


class Camera {
public:

    Camera(vkcomponent::SwapChain& swapChain);
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 inputAxis;

	float pitch{ 0 }; //up-down rotation
	float yaw{ 0 }; //left-right rotation

    float Fov = 70;
	float zNear = 0.1f;
	float zFar = 100.0f;

	bool bSprint = false;
	bool possessCamera = false;


	void ProcessInputEvent(WindowHandler& windowHandler);
	void UpdateCamera(float deltaTime);


	glm::mat4 GetViewMatrix() const;
	glm::mat4 GetProjectionMatrix(bool bReverse = true) const;
	glm::mat4 get_rotation_matrix() const ;
private: 
    vkcomponent::SwapChain* p_swapChain;
};
