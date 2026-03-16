#ifndef IFS_H
#define IFS_H

#include <string>

#include "glm/glm.hpp"

namespace ifs
{
	struct FlameConfig;

	void acquireGLObjects();
	void releaseGLObjects();
	
	void createPreviewTexture();
	
	void updateCam(const glm::vec2& deltaPos, const float deltaZoom);
	void resetCam();
	float getCamZoom();
	bool getPaused();

	void setPreviewTexSize(uint32_t width, uint32_t height);
	void setNumPreviewSamples(uint32_t n);
	void setInitialIterations(uint32_t n);
	void setIterations(uint32_t n);
	void setGamma(float g);
	void setDarkness(float d);

	void addDefaultVariation();
	void addRandomVariation();
	void removeVariation(uint32_t index);
	void setVariationNum(uint32_t index, uint32_t variation);
	void setVariationColour(uint32_t index, glm::vec3 rgb);
	void setVariationWeight(uint32_t index, float w);
	void loadFlameConfig(FlameConfig fc, bool savePrevFlame=true);
	void saveFlameFile();
	void loadFlameFile();


	void createGUI();
	
	bool init(uint32_t tw, uint32_t th);
	void update();
	void clearSamples();
	void draw();
	void render();
	void destroy();

	float randomFloat();
	uint32_t randomVariationNum();
	glm::vec3 randomOKLChtoRGB();

}

#endif