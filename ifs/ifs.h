#ifndef IFS_H
#define IFS_H

#include "glm/glm.hpp"

namespace ifs
{
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
	
	void addDefaultVariation();
	void addRandomVariation();
	void removeVariation(uint32_t index);
	void setVariationNum(uint32_t index, uint32_t variation);
	void setVariationColour(uint32_t index, float L, float C, float h);
	void setVariationWeight(uint32_t index, float w);

	void createGUI();
	
	bool init(uint32_t tw, uint32_t th);
	void update();
	void clearSamples();
	void draw();
	void render();
	void destroy();

	float randomFloat();
	uint32_t randomVariationIndex();
	float* randomOKLCh();
	float* OKLChtoRGB(float oklab[3]);

}

#endif