
#ifndef CAMERA_H
#define CAMERA_H

#include "glm/glm.hpp"

class Camera
{
	//https://learnopengl.com/Getting-started/Camera

	bool initialised;

	void updateViewMatrix();
	void updatePerspective();
	void updateDirection();

public:
	glm::vec3 position, direction;
	glm::vec2 angles; //angles are x: pitch, y: yaw, (no roll)
	glm::mat4 matView, matProj;
	float fov, ar, clipNear, clipFar;

	const glm::mat4 getMatrixWorldToScreen() const { return matProj * matView; }

	void setFieldOfView(const float fieldOfView);
	void setAspectRatio(const float width, const float height);

	void init(const float aspectRatio, const glm::vec3& defaultPos);

	void updatePosition(const glm::vec3& deltaPos);
	void updateViewAngle(const glm::vec2& deltaAngles);
};

#endif //CAMERA_H