
#include "Camera2D.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>


void Camera2D::updateViewMatrix()
{
	matView = glm::translate(glm::rotate(glm::ortho(-view.x, view.x, -view.y, view.y), -angle, glm::vec3(0.0f, 0.0f, 1.0f)), -glm::vec3(position, 0.0f));
}

void Camera2D::setAspectRatio(const float width, const float height)
{
	ar = width / height;
	updateZoom(1.0f);
}

mat4wrap Camera2D::getMatViewCL()
{
	return {
		matView[0][0], matView[1][0], matView[2][0], matView[3][0],
		matView[0][1], matView[1][1], matView[2][1], matView[3][1],
		matView[0][2], matView[1][2], matView[2][2], matView[3][2],
		matView[0][3], matView[1][3], matView[2][3], matView[3][3]
	};
}

void Camera2D::init(const float width, const float height, const glm::vec2& defaultPos)
{
	position = defaultPos;
	zoom = 0.5f;
	setAspectRatio(width, height);
}

void Camera2D::reset()
{
	position = glm::vec2(0.0f);
	zoom = 0.5f;
	angle = 0.0f;
	updateView(ar);
	updateViewMatrix();
}

void Camera2D::updateView(float aspectRatio)
{
	if (aspectRatio > 1.0f)
	{
		view = glm::vec2(aspectRatio, 1.0f) / zoom;
	}
	else
	{
		view = glm::vec2(1.0f, 1.0f / aspectRatio) / zoom;
	}
}

void Camera2D::updatePosition(const glm::vec2& deltaPos)
{
	position += deltaPos;
	updateViewMatrix();
}

void Camera2D::updateZoom(const float deltaZoom)
{
	zoom *= deltaZoom;
	updateView(ar);
	updateViewMatrix();
}

void Camera2D::updateRotation(const float deltaAngle)
{
	angle = glm::mod(angle + deltaAngle, glm::two_pi<float>());
	updateViewMatrix();
}