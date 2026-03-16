
#include "Camera2D.h"

#include "glm/gtc/matrix_transform.hpp"


void Camera2D::updateViewMatrix()
{
	matView = glm::ortho(position.x - view.x, position.x + view.x, position.y - view.y, position.y + view.y);
}

void Camera2D::setAspectRatio(const float width, const float height)
{
	ar = width / height;
	updateView(1.0f);
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

void Camera2D::updatePosition(const glm::vec2& deltaPos)
{
	position += deltaPos;
	updateViewMatrix();
}

void Camera2D::updateView(const float deltaZoom)
{
	zoom *= deltaZoom;
	view = glm::vec2(1.0f / zoom * ar, 1.0f / zoom);
	updateViewMatrix();
}