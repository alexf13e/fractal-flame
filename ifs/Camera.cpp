
#include "Camera.h"

#include "glm/gtc/matrix_transform.hpp"


void Camera::updateViewMatrix()
{
	matView = glm::lookAt(position, position + direction, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::updatePerspective()
{
	matProj = glm::perspective(fov, ar, clipNear, clipFar);
}

void Camera::updateDirection()
{
	direction = glm::vec3(
		glm::cos(angles.y) * glm::cos(angles.x),
		glm::sin(angles.x),
		glm::sin(angles.y) * glm::cos(angles.x)
	);

	updateViewMatrix();
}

void Camera::setFieldOfView(const float fieldOfView)
{
	fov = fieldOfView;
	updatePerspective();
}

void Camera::setAspectRatio(const float width, const float height)
{
	ar = width / height;
	updatePerspective();
}

void Camera::init(const float aspectRatio, const glm::vec3& defaultPos)
{
	initialised = false;

	position = defaultPos;
	angles = glm::vec2(0.0f, -glm::half_pi<float>()); //0,0 = positive X

	updateDirection();

	//default perspective values
	fov = glm::radians(90.0f);
	ar = aspectRatio;
	clipNear = 0.001f;
	clipFar = 100.0f;
	updatePerspective();

	initialised = true;
}

void Camera::updatePosition(const glm::vec3& deltaPos)
{
	position += deltaPos;
	updateViewMatrix();
}

void Camera::updateViewAngle(const glm::vec2& deltaAngles)
{
	constexpr float rad90deg = glm::radians(89.0f); //cap a little before 90 degrees to prevent issues
	angles.x = glm::min(glm::max(angles.x + deltaAngles.x, -rad90deg), rad90deg);
	angles.y = glm::mod(angles.y + deltaAngles.y, 2.0f * glm::pi<float>());
	updateDirection();
}