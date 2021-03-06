#pragma once

#include <glm/gtx/transform.hpp>
#include <glm/glm.hpp>
#include "renderer.h"

#include <string>

class Camera {
public:
	Camera();

	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 velocity;
	glm::vec3 inputAxis;

	glm::vec3 forward;
	glm::vec3 up;

	glm::vec4 viewPos;

	float pitch{ 0 }; //up-down rotation
	float yaw{ 0 }; //left-right rotation

    float Fov = 70;
	float zNear = 0.1f;
	float zFar = 100.0f;

	bool bSprint = false;
	bool possessCamera = false;
	


	void ProcessInputEvent();
	void UpdateCamera(const float& deltaTime);
	void RenderCamera();

	const std::string GetCameraBuffer(){return "camera buffer";}


	glm::mat4 GetViewMatrix() const;
	glm::mat4 GetProjectionMatrix(bool bReverse = false, bool flipY =true) const;
	glm::mat4 GetRotationMatrix() const;

	bool& GetIsActive() {return isActive;}
	void SetIsActive(const bool& input);
private:
	bool isActive = false;
};

struct CameraManager
{
	static void Init();
	static void AddCamera(const std::string& cameraName);
	static Camera& GetCamera(const std::string& cameraName);
	static Camera& GetActiveCamera();
	static void Render();
	static void UpdateInput(const float& deltaTime);
	static void Destroy();
};



