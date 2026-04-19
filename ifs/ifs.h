#ifndef IFS_H
#define IFS_H

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Camera2D.h"


namespace ifs
{
	struct FlameConfig;

	void acquireGLObjects();
	void releaseGLObjects();
	
	void createPreviewTexture();
	
	void updateCam(const glm::vec2& deltaPos, const float deltaZoom, const float deltaAngle);
	void updateCamPositionMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen);
	void updateCamZoomMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen);
	void updateCamRotationMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen);
	void resetCam();
	
	float getCamZoom();
	float getCamAngle();
	bool getPaused();
	bool getMaxPreviewFramesReached();
	void togglePause();
	void updatePausedCamMatrix();


	void setPreviewTexSize(uint32_t width, uint32_t height);
	void setMaxPreviewFrames(uint32_t n);
	void setInitialIterations(uint32_t n);
	void setDrawingIterations(uint32_t n);
	
	void setBrightness(float b);
	void setIntensity(float v);
	void setGamma(float g);

	void enableDrawMouseLine(const glm::vec2& currentMousePosScreen);

	void addDefaultVariation();
	void addRandomVariation();
	void removeVariation(uint32_t index);
	
	void setVariationNum(uint32_t index, uint32_t variation);
	void setVariationcolor(uint32_t index, glm::vec3 rgb);
	void setVariationWeight(uint32_t index, float w);
	void updateVariationTransform(uint32_t index);
	void setVariationTranslation(uint32_t index, const glm::vec2& t);
	void setVariationRotation(uint32_t index, const float r);
	void setVariationScale(uint32_t index, const glm::vec2& s);

	void loadFlameConfig(FlameConfig fc, bool savePrevFlame=true);
	void saveFlameFile();
	void loadFlameFile();

	void createGUI(const float frameDuration);
	
	bool init(uint32_t tw, uint32_t th);
	void updateKernelParams();
	void update();
	void clearSamples();
	void draw();
	
	float randomFloat();
	uint32_t randomVariationNum();
	glm::vec3 randomOKLChtoRGB();

	void appendInfo(const std::string& s);
	void replaceInfo(const uint32_t i, const std::string& s);
}

namespace FileRender
{
	namespace
	{
		struct Tile
		{
			uint32_t width, height;
			mat4wrap matView;
		};

		bool checkImageCanRender(uint32_t width, uint32_t height);
		std::string getStringTimestamp();
		std::vector<Tile> getTiles();

		void taskSaveProcessedImage();
		void taskSaveUnprocessedData();
	}

	bool getRunning();
	bool getCancelled();
	void cancel();

	void saveProcessedImage();
	void saveUnprocessedData();
}

#endif
