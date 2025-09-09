
#ifndef CAMERA2D_H
#define CAMERA2D_H

#include "glm/glm.hpp"

struct mat4wrap
{
	float s[16];
};

class Camera2D
{
	void updateViewMatrix();

public:
	glm::vec2 position, view;
	glm::mat4 matView;
	float ar;
	float zoom;

	void setAspectRatio(const float width, const float height);
	mat4wrap getMatViewCL();

	void init(const float width, const float height, const glm::vec2& defaultPos);
	void reset();

	void updatePosition(const glm::vec2& deltaPos);
	void updateView(const float deltaZoom);
};

#endif //CAMERA2D_H