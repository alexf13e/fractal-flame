
#include "ifs.h"

#include <ctime>
#include <stack>
#include <fstream>
#include <sstream>
#include <list>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "imgui.h"

#include "json.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vector_angle.hpp>

#define CL_MANAGER_IMPL
#define CL_MANAGER_GL
#include "CLManager.h"
#include "Camera2D.h"
#include "ShaderProgram.h"
#include "filedialog.h"

#include "common_def.h"

#undef min
#undef max


namespace ifs
{
	struct FlameConfig
	{
		uint32_t numVariations;
		uint32_t variations[MAX_VARIATIONS];
		float colors[MAX_VARIATIONS * 3];
		float weights[MAX_VARIATIONS];
		float transforms[MAX_VARIATIONS * 6];

		glm::vec2 translations[MAX_VARIATIONS];
		float rotations[MAX_VARIATIONS];
		glm::vec2 scales[MAX_VARIATIONS];
	};

	namespace
	{
		ShaderProgram shFullScreenTri, shLines;
		GLuint vao_fullScreenTri;
		GLuint vao_lines, vbo_guideLines2x2, vbo_guideLines3x3, vbo_mouseLine;

		std::string b_previewTexture = "previewTexture";
		std::string glb_previewTextureProcessed = "previewTextureProcessed";

		std::string b_renderTexture = "renderTexture";
		std::string b_renderTextureBytes = "renderTextureBytes";

		std::string b_variations = "variations";
		std::string b_colors = "colors";
		std::string b_weights = "weights";
		std::string b_transforms = "transforms";

		std::string k_produceSamples = "produceSamples";
		std::string k_postProcess = "postProcess";
		std::string k_floatToByte = "floatToByte";

		std::vector<cl::Memory> glObjectsToAcquire;

		
		uint32_t numSampleThreads;
		uint32_t maxPreviewFrames;
		uint32_t numRenderFrames;
		uint32_t initialIterations;
		uint32_t drawingIterations;
		
		Camera2D cam;
		glm::vec2 lastRenderedCamPos;
		float lastRenderedCamZoom;
		float lastRenderedCamAngle;
		glm::mat4 matPausedCamTransform;
		uint32_t guideLineNum;

		uint32_t previewTexWidth, previewTexHeight;
		uint32_t renderTexWidth, renderTexHeight;
		bool renderMatchPreview;
		uint8_t renderNextFrame, saveUnprocessedDataNextFrame;

		float brightness;
		float intensity;
		float gamma;
		
		bool paused;
		bool clearEveryFrame;
		bool clearSingleFrame;
		bool plotWithoutAtomic;
		bool wantsPostProcess;
		bool drawMouseLine;
		bool clearOnUnpause;
		bool showGUI;


		FlameConfig currentFlame;
		std::stack<FlameConfig> previousFlames;

		uint32_t frameNum = 0;

		uint32_t VALID_VARIATIONS[] = {
			0,
			1,
			2,
			3,
			4,
			5,
			6,
			7,
			8,
			9,
			10,
			11,
			12,
			13,
			14,
			15,
			16,
			17,
			18,
			19,
			20,
			21,
			22,
			27,
			28,
			29,
			31,
			34,
			35,
			42,
			43,
			48,
			100,
			101
		};

		std::unordered_map<uint32_t, const char*> VARIATION_NAMES = {
			{ 0,	"Linear" },
			{ 1,	"Sinusoidal" },
			{ 2,	"Spherical" },
			{ 3,	"Swirl" },
			{ 4,	"Horseshoe" },
			{ 5,	"Polar" },
			{ 6,	"Handkerchief" },
			{ 7,	"Heart" },
			{ 8,	"Disc" },
			{ 9,	"Spiral" },
			{ 10,	"Hyperbolic" },
			{ 11,	"Diamond" },
			{ 12,	"Ex" },
			{ 13,	"Julia" },
			{ 14,	"Bent" },
			{ 15,	"Waves" },
			{ 16,	"Fisheye (y,x)" },
			{ 17,	"Popcorn" },
			{ 18,	"Exponential" },
			{ 19,	"Power" },
			{ 20,	"Cosine" },
			{ 21,	"Rings" },
			{ 22,	"Fan" },
			{ 27,	"Eyefish (x,y)" },
			{ 28,	"Bubble" },
			{ 29,	"Cylinder" },
			{ 31,	"Noise" },
			{ 34,	"Blur" },
			{ 35,	"Gaussian" },
			{ 42,	"Tangent" },
			{ 43,	"Square" },
			{ 48,	"Cross" },
			{ 100,	"Sierpinski" },
			{ 101,	"Menger" }
		};

		uint32_t NUM_VALID_VARIATIONS = VARIATION_NAMES.size();

		std::unordered_map<uint32_t, const char*> GUIDELINE_NAMES = {
			{0, "None"},
			{1, "2x2"},
			{2, "3x3"}
		};

		uint32_t NUM_GUIDELINE_NAMES = GUIDELINE_NAMES.size();


		constexpr float MIN_CAMERA_ZOOM = 0.001f;
		constexpr float MAX_CAMERA_ZOOM = 50.0f;

		constexpr float MIN_THRESHOLD = 0.0f;
		constexpr float MAX_THRESHOLD = 10.0f;

		constexpr float DEFAULT_BRIGHTNESS = 0.5f;
		constexpr float MIN_BRIGHTNESS = 0.0f;
		constexpr float MAX_BRIGHTNESS = 2.0f;

		constexpr float DEFAULT_INTENSITY = 1.0f;
		constexpr float MIN_INTENSITY = 0.0f;
		constexpr float MAX_INTENSITY = 1.0f;

		constexpr float DEFAULT_GAMMA = 2.2f;
		constexpr float MIN_GAMMA = 0.1f;
		constexpr float MAX_GAMMA = 5.0f;

		constexpr int MIN_IMAGE_SIZE = 32;
		constexpr int MAX_IMAGE_SIZE = 16384;

		constexpr uint32_t infoLength = 5;
		std::list<std::string> info;
	}

	void acquireGLObjects()
	{
		int error = CLManager::queue.enqueueAcquireGLObjects(&glObjectsToAcquire);
		if (error != CL_SUCCESS)
		{
			std::cout << "error acquiring GL object: " << CLManager::getErrorString(error) << std::endl;
		}
	}

	void releaseGLObjects()
	{
		int error = CLManager::queue.enqueueReleaseGLObjects(&glObjectsToAcquire);
		if (error != CL_SUCCESS)
		{
			std::cout << "error releasing GL object: " << CLManager::getErrorString(error) << std::endl;
		}
	}

	void createPreviewTexture()
	{
		glObjectsToAcquire.clear();

		//replace preview buffer
		uint32_t numPixels = previewTexWidth * previewTexHeight;
		CLManager::createBuffer<float>(b_previewTexture, numPixels * 4);
		CLManager::createGLBufferNoVAO<float>(glb_previewTextureProcessed, GL_SHADER_STORAGE_BUFFER, numPixels * 4);

		glUseProgram(shFullScreenTri.getID());
		int bufferBlockBinding = 0;
		int bufferBlockIndex = glGetProgramResourceIndex(shFullScreenTri.getID(), GL_SHADER_STORAGE_BLOCK, "TexOutput");
		glShaderStorageBlockBinding(shFullScreenTri.getID(), bufferBlockIndex, bufferBlockBinding);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferBlockBinding, CLManager::glBuffers[glb_previewTextureProcessed].glBuffer);

		glUniform1ui(glGetUniformLocation(shFullScreenTri.getID(), "texWidth"), previewTexWidth);
		glUniform1ui(glGetUniformLocation(shFullScreenTri.getID(), "texHeight"), previewTexHeight);
		glUseProgram(0);

		glObjectsToAcquire.push_back(CLManager::glBuffers[glb_previewTextureProcessed].clBuffer);
	}

	void updateCam(const glm::vec2& deltaPos, const float deltaZoom, const float deltaAngle)
	{
		cam.updatePosition(deltaPos);
		cam.updateZoom(deltaZoom);
		cam.updateRotation(deltaAngle);
		
		if (paused) updatePausedCamMatrix();
		else clearSingleFrame = true;
	}

	void updateCamPositionMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen)
	{
		glm::vec2 currentMousePosNDC = currentMousePosScreen / glm::vec2(previewTexWidth, previewTexHeight) * 2.0f - 1.0f;
		glm::vec2 prevMousePosNDC = prevMousePosScreen / glm::vec2(previewTexWidth, previewTexHeight) * 2.0f - 1.0f;
		currentMousePosNDC.y = 1.0f - currentMousePosNDC.y;
		prevMousePosNDC.y = 1.0f - prevMousePosNDC.y;
		currentMousePosNDC *= cam.view;
		prevMousePosNDC *= cam.view;

		//pan view so that the position under the mouse stays attached to the mouse
		//camera wants to move opposite direction to mouse, so invert
		glm::vec2 deltaPos = prevMousePosNDC - currentMousePosNDC;
		deltaPos = glm::rotateZ(glm::vec3(deltaPos, 0.0f), cam.angle);
		if (glm::length2(deltaPos) > 0.0f)
		{
			cam.updatePosition(deltaPos);
			if (paused) updatePausedCamMatrix();
			else clearSingleFrame = true;
		}
	}

	void updateCamZoomMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen)
	{
		//rotate and zoom the view such that the point under the mouse stays attached to the mouse, and the centre of the screen stays at the centre
		//the mouse point wants to be NDC so that 0,0 is in screen centre
		glm::vec2 currentMousePosNDC = currentMousePosScreen / glm::vec2(previewTexWidth, previewTexHeight) * 2.0f - 1.0f;
		glm::vec2 prevMousePosNDC = prevMousePosScreen / glm::vec2(previewTexWidth, previewTexHeight) * 2.0f - 1.0f;
		currentMousePosNDC *= cam.view;
		prevMousePosNDC *= cam.view;

		float deltaZoom = 1.0f;
		float prevMouseLength = glm::length(prevMousePosNDC);
		if (prevMouseLength > 0.0f)
		{
			deltaZoom = glm::length(currentMousePosNDC) / prevMouseLength;
		}

		if (deltaZoom != 1.0f)
		{
			cam.updateZoom(deltaZoom);
			if (paused) updatePausedCamMatrix();
			else clearSingleFrame = true;
		}
	}

	void updateCamRotationMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen)
	{
		glm::vec2 currentMousePosNDC = currentMousePosScreen / glm::vec2(previewTexWidth, previewTexHeight) * 2.0f - 1.0f;
		glm::vec2 prevMousePosNDC = prevMousePosScreen / glm::vec2(previewTexWidth, previewTexHeight) * 2.0f - 1.0f;
		currentMousePosNDC *= cam.view;
		prevMousePosNDC *= cam.view;

		float prevMouseLength = glm::length(prevMousePosNDC);
		float currentMouseLength = glm::length(currentMousePosNDC);

		float deltaAngle = 0.0f;
		if (currentMouseLength > 0.0f && prevMouseLength > 0.0f)
		{
			float cosTheta = glm::dot(currentMousePosNDC / currentMouseLength, prevMousePosNDC / prevMouseLength);
			if (cosTheta < 1.0f)
			{
				deltaAngle = -glm::acos(cosTheta) * glm::sign(glm::determinant(glm::mat2(currentMousePosNDC, prevMousePosNDC)));
			}
		}

		if (deltaAngle != 0.0f)
		{
			cam.updateRotation(deltaAngle);
			if (paused) updatePausedCamMatrix();
			else clearSingleFrame = true;
		}
	}

	void resetCam()
	{
		cam.reset();
		if (paused) updatePausedCamMatrix();
		else clearSingleFrame = true;
	}

	void togglePause()
	{
		paused = !paused;
		
		glUseProgram(shFullScreenTri.getID());

		if (paused)
		{
			lastRenderedCamPos = cam.position;
			lastRenderedCamZoom = cam.zoom;
			lastRenderedCamAngle = cam.angle;
			matPausedCamTransform = glm::mat4(1.0f);
			glUniform1i(glGetUniformLocation(shFullScreenTri.getID(), "paused"), 1);
			glUniformMatrix4fv(glGetUniformLocation(shFullScreenTri.getID(), "matPausedCamTransform"), 1, GL_FALSE, &matPausedCamTransform[0][0]);
		}
		else
		{
			glUniform1i(glGetUniformLocation(shFullScreenTri.getID(), "paused"), 0);

			if (clearOnUnpause)
			{
				clearSingleFrame = true;
				clearOnUnpause = false;
			}
		}

		glUseProgram(0);
	}

	void updatePausedCamMatrix()
	{
		glm::vec2 deltaPos = glm::rotateZ(glm::vec3(cam.position - lastRenderedCamPos, 0.0f), -cam.angle);
		float deltaZoom = cam.zoom / lastRenderedCamZoom;
		float deltaAngle = cam.angle - lastRenderedCamAngle;
		glm::vec2 view;
		if (cam.ar > 1.0f) view = glm::vec2(cam.ar, 1.0f);
		else view = glm::vec2(1.0f, 1.0f / cam.ar);

		glm::mat4 matPausedCamTransform = glm::mat4(1.0f);
		matPausedCamTransform = glm::translate(matPausedCamTransform, glm::vec3(0.5f, 0.5f, 0.5f));
		matPausedCamTransform = glm::scale(matPausedCamTransform, 1.0f / glm::vec3(view, 1.0f));
		matPausedCamTransform = glm::rotate(matPausedCamTransform, deltaAngle, glm::vec3(0.0f, 0.0f, 1.0f));
		matPausedCamTransform = glm::scale(matPausedCamTransform, 1.0f / glm::vec3(deltaZoom, deltaZoom, 1.0f));
		matPausedCamTransform = glm::translate(matPausedCamTransform, glm::vec3(deltaPos * cam.zoom * 0.5f, 0.0f));
		matPausedCamTransform = glm::scale(matPausedCamTransform, glm::vec3(view, 1.0f));
		matPausedCamTransform = glm::translate(matPausedCamTransform, -glm::vec3(0.5f, 0.5f, 0.5f));

		glUseProgram(shFullScreenTri.getID());
		glUniformMatrix4fv(glGetUniformLocation(shFullScreenTri.getID(), "matPausedCamTransform"), 1, GL_FALSE, &matPausedCamTransform[0][0]);
		glUseProgram(0);

		clearOnUnpause = true;
	}

	float getCamZoom()
	{
		return cam.zoom;
	}

	float getCamAngle()
	{
		return cam.angle;
	}

	bool getPaused()
	{
		return paused;
	}

	bool getMaxPreviewFramesReached()
	{
		return (maxPreviewFrames != 0 && frameNum == maxPreviewFrames) || currentFlame.numVariations == 0;
	}

	void setPreviewTexSize(uint32_t width, uint32_t height)
	{
		//resize the preview buffer (usually to match window after it is resized)
		previewTexWidth = width;
		previewTexHeight = height;
		if (renderMatchPreview)
		{
			renderTexWidth = width;
			renderTexHeight = height;
		}

		createPreviewTexture();
		cam.setAspectRatio(previewTexWidth, previewTexHeight);
	}

	void setMaxPreviewFrames(uint32_t n)
	{
		maxPreviewFrames = n;
		if (maxPreviewFrames > 0 && maxPreviewFrames < frameNum)
		{
			clearSingleFrame = true;
		}

		if (paused) togglePause();
	}

	void setInitialIterations(uint32_t n)
	{
		//number of iterations which will run on sample points before their positions are drawn to the buffer
		initialIterations = n;
		clearSingleFrame = true;
		if (paused) togglePause();
	}

	void setDrawingIterations(uint32_t n)
	{
		//number of iterations after top of initialIterations, where the sample position at each iteration WILL be drawn
		drawingIterations = n;
		clearSingleFrame = true;
		if (paused) togglePause();
	}

	void setBrightness(float b)
	{
		//brightness is multiplied with pixel value before gamma
		brightness = b;
		wantsPostProcess = true;
	}

	void setIntensity(float v)
	{
		//intensity blends between
		//	0: pow(pix.xyz, 1.0f / gamma)
		//	1: pow(pix.xyz, 1.0f / gamma) * pix.w * brightness 
		intensity = v;
		wantsPostProcess = true;
	}

	void setGamma(float g)
	{
		//pixel value is raised to power of 1/gamma
		gamma = g;
		wantsPostProcess = true;
	}

	void enableDrawMouseLine(const glm::vec2& currentMousePosScreen)
	{
		glm::vec2 currentMousePosNDC = currentMousePosScreen / glm::vec2(previewTexWidth, previewTexHeight) * 2.0f - 1.0f;

		drawMouseLine = true;
		//set line to be drawn from screen centre to mouse as visual aid
		std::vector<float> mouseLine = { 0.0f, 0.0f, currentMousePosNDC.x, -currentMousePosNDC.y };
		glBindVertexArray(vao_lines);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_mouseLine);
		glBufferData(GL_ARRAY_BUFFER, mouseLine.size() * sizeof(float), mouseLine.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void addDefaultVariation()
	{
		//shortcut for adding a new variation with some parameters
		if (currentFlame.numVariations < MAX_VARIATIONS)
		{
			uint32_t index = currentFlame.numVariations;
			currentFlame.numVariations++;

			glm::vec3 rgb = { 1.0f, 1.0f, 1.0f };
			setVariationNum(index, 0);
			setVariationcolor(index, rgb);
			setVariationWeight(index, 1.0f);
			setVariationTranslation(index, glm::vec2(0.0f));
			setVariationRotation(index, 0.0f);
			setVariationScale(index, glm::vec2(1.0f));

			if (paused) clearOnUnpause = true;
			else clearSingleFrame = true;
		}
	}

	void addRandomVariation()
	{
		//shortcut for adding variation with randomised parameters
		if (currentFlame.numVariations < MAX_VARIATIONS)
		{
			uint32_t index = currentFlame.numVariations;
			currentFlame.numVariations++;

			setVariationNum(index, VALID_VARIATIONS[randomVariationNum()]);
			glm::vec3 rgb = randomOKLChtoRGB();
			setVariationcolor(index, rgb);
			setVariationWeight(index, randomFloat());
			setVariationTranslation(index, glm::vec2(randomFloat() * 2.0f - 1.0f, randomFloat() * 2.0f - 1.0f));
			setVariationRotation(index, randomFloat() * glm::two_pi<float>());
			setVariationScale(index, glm::vec2(randomFloat(), randomFloat()));

			clearSingleFrame = true;
			if (paused) togglePause();
		}
	}

	void removeVariation(uint32_t index)
	{
		//shift variations down after the deleted one
		for (uint32_t j = index; j < currentFlame.numVariations - 1; j++)
		{
			currentFlame.variations[j] = currentFlame.variations[j + 1];
			currentFlame.colors[j * 3 + 0] = currentFlame.colors[(j + 1) * 3 + 0];
			currentFlame.colors[j * 3 + 1] = currentFlame.colors[(j + 1) * 3 + 1];
			currentFlame.colors[j * 3 + 2] = currentFlame.colors[(j + 1) * 3 + 2];
			currentFlame.weights[j] = currentFlame.weights[j + 1];
			currentFlame.translations[j] = currentFlame.translations[j + 1];
			currentFlame.rotations[j] = currentFlame.rotations[j + 1];
			currentFlame.scales[j] = currentFlame.scales[j + 1];
			currentFlame.transforms[j * 6 + 0] = currentFlame.transforms[(j + 1) * 6 + 0];
			currentFlame.transforms[j * 6 + 1] = currentFlame.transforms[(j + 1) * 6 + 1];
			currentFlame.transforms[j * 6 + 2] = currentFlame.transforms[(j + 1) * 6 + 2];
			currentFlame.transforms[j * 6 + 3] = currentFlame.transforms[(j + 1) * 6 + 3];
			currentFlame.transforms[j * 6 + 4] = currentFlame.transforms[(j + 1) * 6 + 4];
			currentFlame.transforms[j * 6 + 5] = currentFlame.transforms[(j + 1) * 6 + 5];
		}

		currentFlame.numVariations--;

		//update kernel buffer parameters
		CLManager::writeBuffer(b_variations, MAX_VARIATIONS, currentFlame.variations);
		CLManager::writeBuffer(b_colors, MAX_VARIATIONS * 3, currentFlame.colors);
		CLManager::writeBuffer(b_weights, MAX_VARIATIONS, currentFlame.weights);
		CLManager::writeBuffer(b_transforms, MAX_VARIATIONS * 6, currentFlame.transforms);

		clearSingleFrame = true;
		if (paused) togglePause();
	}

	void setVariationNum(uint32_t index, uint32_t variation)
	{
		if (index >= currentFlame.numVariations) return;

		bool valid = false;
		for (uint32_t i = 0; i < NUM_VALID_VARIATIONS; i++)
		{
			if (variation == VALID_VARIATIONS[i])
			{
				valid = true;
				break;
			}
		}

		if (!valid)
		{
			appendInfo("Tried to set invalid variation: " + std::to_string(variation));
			return;
		}

		currentFlame.variations[index] = variation;
		CLManager::writeBuffer(b_variations, 1, &currentFlame.variations[index], index);
		clearSingleFrame = true;
		if (paused) togglePause();
	}

	void setVariationcolor(uint32_t index, glm::vec3 rgb)
	{
		if (index >= currentFlame.numVariations) return;

		currentFlame.colors[index * 3 + 0] = rgb.x;
		currentFlame.colors[index * 3 + 1] = rgb.y;
		currentFlame.colors[index * 3 + 2] = rgb.z;

		CLManager::writeBuffer(b_colors, 3, &currentFlame.colors[index * 3], index * 3);
		clearSingleFrame = true;
		if (paused) togglePause();
	}

	void setVariationWeight(uint32_t index, float w)
	{
		if (index >= currentFlame.numVariations) return;

		currentFlame.weights[index] = w;
		CLManager::writeBuffer(b_weights, 1, &currentFlame.weights[index], index);
		clearSingleFrame = true;
		if (paused) togglePause();
	}

	void updateVariationTransform(uint32_t index)
	{
		if (index >= currentFlame.numVariations) return;

		glm::mat3 transform =
			glm::rotate(
				glm::translate(
					glm::scale(glm::mat3(1.0f), currentFlame.scales[index]),
					currentFlame.translations[index]),
				currentFlame.rotations[index]);

		currentFlame.transforms[index * 6 + 0] = transform[0][0];
		currentFlame.transforms[index * 6 + 1] = transform[1][0];
		currentFlame.transforms[index * 6 + 2] = transform[2][0];
		currentFlame.transforms[index * 6 + 3] = transform[0][1];
		currentFlame.transforms[index * 6 + 4] = transform[1][1];
		currentFlame.transforms[index * 6 + 5] = transform[2][1];

		CLManager::writeBuffer(b_transforms, 6, &currentFlame.transforms[index * 6], index * 6);
		clearSingleFrame = true;
		if (paused) togglePause();
	}

	void setVariationTranslation(uint32_t index, const glm::vec2& t)
	{
		if (index >= currentFlame.numVariations) return;

		currentFlame.translations[index] = t;
		updateVariationTransform(index);
	}

	void setVariationRotation(uint32_t index, const float r)
	{
		if (index >= currentFlame.numVariations) return;

		float newRotation = glm::mod(r, glm::two_pi<float>());
		if (newRotation == currentFlame.rotations[index]) return;

		currentFlame.rotations[index] = newRotation;
		updateVariationTransform(index);
	}

	void setVariationScale(uint32_t index, const glm::vec2& s)
	{
		if (index >= currentFlame.numVariations) return;

		currentFlame.scales[index] = s;
		updateVariationTransform(index);
	}

	void loadFlameConfig(FlameConfig fc, bool savePrevFlame)
	{
		if (savePrevFlame) previousFlames.push(currentFlame);

		currentFlame = fc;

		CLManager::writeBuffer(b_variations, fc.numVariations, currentFlame.variations);
		CLManager::writeBuffer(b_colors, fc.numVariations * 3, currentFlame.colors);
		CLManager::writeBuffer(b_weights, fc.numVariations, currentFlame.weights);

		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			updateVariationTransform(i);
		}

		clearSingleFrame = true;
		if (paused) togglePause();
	}

	void saveFlameFile()
	{
		std::string fileName = "config";
		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			fileName += "_" + std::to_string(currentFlame.variations[i]);
		}

		std::vector<nfdu8filteritem_t> filters = { { "Flame config", "json" } };
		std::string fileDir = FileDialog::saveDialog(fileName, filters);
		if (fileDir == "")
		{
			//user closed the file dialog
			return;
		}

		std::ofstream fileStream(fileDir);
		if (!fileStream.is_open())
		{
			appendInfo("Failed to save flame config to file: " + fileDir);
			return;
		}

		nlohmann::json outputData;
		outputData["variations"] = nlohmann::json::array();
		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			outputData["variations"].push_back(
			{
				{ "variation", currentFlame.variations[i] },
				{ "color", {currentFlame.colors[i * 3], currentFlame.colors[i * 3 + 1], currentFlame.colors[i * 3 + 2]} },
				{ "weight", currentFlame.weights[i] },
				{ "translation", {currentFlame.translations[i].x, currentFlame.translations[i].y} },
				{ "rotation", currentFlame.rotations[i] },
				{ "scale", {currentFlame.scales[i].x, currentFlame.scales[i].y} }
			});
		}

		outputData["camera"] =
		{
			{ "position", {cam.position.x, cam.position.y} },
			{ "angle", cam.angle },
			{ "zoom", cam.zoom }
		};

		outputData["colorProcessing"] = 
		{
			{ "brightness", brightness },
			{ "intensity", intensity },
			{ "gamma", gamma },
		};

		fileStream << std::setw(4) << outputData;
	}

	void loadFlameFile()
	{
        //i love user input error handling and feedback

        //helper functions
		auto checkValuePresent = [](const nlohmann::json& data, const std::string& name) {
            if (!data.contains(name))
            {
                appendInfo("Loading cancelled - missing required data: " + name);
                return false;
            }

            return true;
		};

		auto checkValueFloat = [](const nlohmann::json& data, const std::string& name, float min, float max, float* dest) {
			float v;
			try
			{
				v = data[name].get<float>();
			}
			catch (nlohmann::json::type_error e)
			{
				return false;
			}

			if (v < min || v > max)
			{
				return false;
			}

			*dest = v;
			return true;
		};

        auto checkValueFloatArray = [](const nlohmann::json& data, const std::string& name, float min, float max, std::vector<float>* dest) {
			std::vector<float> values;
			try
			{
				values = data[name].get<std::vector<float>>();
			}
			catch (nlohmann::json::type_error e)
			{
				return false;
			}

            for (const float v : values)
            {
                if (v < min || v > max)
                {
                    return false;
                }
            }

			*dest = values; //annoying copy but only being used for arrays of 2-3 elements
			return true;
		};

        //helper functions for the helper functions
        auto checkVariationValuePresent = [](const nlohmann::json& data, const std::string& name, const uint32_t variationIndex) {
            if (!data.contains(name))
            {
                appendInfo("Loading cancelled - missing " + name + " in variation " + std::to_string(variationIndex));
                return false;
            }

            return true;
        };

		auto checkVariationNum = [](const nlohmann::json& data, uint32_t* dest, const uint32_t variationIndex) {
			int variationNum;
			try
			{
				variationNum = data["variation"].get<int>();
			}
			catch (nlohmann::json::type_error e)
			{
                appendInfo("Loading cancelled - invalid variation num in variation " + std::to_string(variationIndex));
				return false;
			}

			bool variationNumValid = false;
			for (uint32_t validVariationNum : VALID_VARIATIONS)
			{
				if (variationNum == validVariationNum)
				{
					variationNumValid = true;
					break;
				}
			}

			if (!variationNumValid)
			{
                appendInfo("Loading cancelled - invalid variation num in variation " + std::to_string(variationIndex));
				return false;
			}

			*dest = variationNum;
			return true;
		};

		auto checkVariationValueFloat = [&checkValueFloat](const nlohmann::json& data, const std::string& name, float min, float max, float* dest, const uint32_t variationIndex) {
            if (!checkValueFloat(data, name, min, max, dest))
            {
                appendInfo("Loading cancelled - invalid " + name + " in variation " + std::to_string(variationIndex));
                return false;
            }

            return true;
        };

        auto checkVariationValueFloatArray = [&checkValueFloatArray](const nlohmann::json& data, const std::string& name, float min, float max, std::vector<float>* dest, const uint32_t variationIndex) {
            if (!checkValueFloatArray(data, name, min, max, dest))
            {
                appendInfo("Loading cancelled - invalid " + name + " in variation " + std::to_string(variationIndex));
                return false;
            }

            return true;
        };

        auto checkCamValuePresent = [](const nlohmann::json& data, const std::string& name) {
            if (!data["camera"].contains(name))
            {
                appendInfo("Loading cancelled - missing " + name + " in camera settings");
                return false;
            }

            return true;
        };

        auto checkCamValueFloat = [&checkValueFloat](const nlohmann::json& data, const std::string& name, float min, float max, float* dest) {
            if (!checkValueFloat(data["camera"], name, min, max, dest))
            {
                appendInfo("Loading cancelled - invalid " + name + " in camera settings");
                return false;
            }

            return true;
        };

        auto checkCamValueFloatArray = [&checkValueFloatArray](const nlohmann::json& data, const std::string& name, float min, float max, std::vector<float>* dest) {
            if (!checkValueFloatArray(data["camera"], name, min, max, dest))
            {
                appendInfo("Loading cancelled - invalid " + name + "in camera settings");
                return false;
            }

            return true;
        };

        auto checkColorValuePresent = [](const nlohmann::json& data, const std::string& name) {
            if (!data["colorProcessing"].contains(name))
            {
                appendInfo("Loading cancelled - missing " + name + " in color processing settings");
                return false;
            }

            return true;
        };

        auto checkColorValueFloat = [&checkValueFloat](const nlohmann::json& data, const std::string& name, float min, float max, float* dest) {
            if (!checkValueFloat(data["colorProcessing"], name, min, max, dest))
            {
                appendInfo("Loading cancelled - invalid " + name + " in color processing settings");
                return false;
            }

            return true;
        };

        auto checkColorValueFloatArray = [&checkValueFloatArray](const nlohmann::json& data, const std::string& name, float min, float max, std::vector<float>* dest) {
            if (!checkValueFloatArray(data["colorProcessing"], name, min, max, dest))
            {
                appendInfo("Loading cancelled - invalid " + name + "in color processing settings");
                return false;
            }

            return true;
        };


		std::vector<nfdu8filteritem_t> filters = { { "Flame config", "json" } };
		std::string fileDir = FileDialog::openDialog(filters);
		if (fileDir == "")
		{
			//user closed the file dialog
			return;
		}

		std::ifstream fileStream(fileDir);
		if (!fileStream.is_open())
		{
			appendInfo("Loading cancelled - failed to access flame config file: " + fileDir);
			return;
		}
		
        //read the file in, deal with error if it is not valid json
		nlohmann::json inputData;
        try
        {
            fileStream >> inputData;
        }
        catch (nlohmann::json::parse_error e)
        {
            appendInfo("Loading cancelled - invalid json file (see terminal for details)");
            std::cout << e.what() << std::endl;
            return;
        }

        //temporary config to hold data while it is loaded
		FlameConfig newFlameConfig{ 0 };

		if (inputData.size() == 0)
		{
			//file just has empty json brackets, interpret as wanting to have no variations and default camera and colour settings
			loadFlameConfig(newFlameConfig);
            cam.reset();
            setBrightness(DEFAULT_BRIGHTNESS);
            setIntensity(DEFAULT_INTENSITY);
            setGamma(DEFAULT_GAMMA);
			return;
		}

        //make sure all required values are present
        if (!checkValuePresent(inputData, "variations")) return;
        if (!checkValuePresent(inputData, "camera")) return;
        if (!checkValuePresent(inputData, "colorProcessing")) return;

        if (!checkCamValuePresent(inputData, "position")) return;
        if (!checkCamValuePresent(inputData, "angle")) return;
        if (!checkCamValuePresent(inputData, "zoom")) return;

        if (!checkColorValuePresent(inputData, "brightness")) return;
        if (!checkColorValuePresent(inputData, "intensity")) return;
        if (!checkColorValuePresent(inputData, "gamma")) return;

        if (inputData["variations"].size() >= MAX_VARIATIONS)
        {
            appendInfo("Loading cancelled - file contained more than the maximum number of variations");
            return;
        }        

        std::vector<float> tempArray;
        for (const auto& variationDataPair : inputData["variations"].items())
        {
            uint32_t variationIndex = newFlameConfig.numVariations;
            const nlohmann::json& variationData = variationDataPair.value();

            //check variation settings are present
            if (!checkVariationValuePresent(variationData, "variation", variationIndex)) return;
            if (!checkVariationValuePresent(variationData, "color", variationIndex)) return;
            if (!checkVariationValuePresent(variationData, "weight", variationIndex)) return;
            if (!checkVariationValuePresent(variationData, "translation", variationIndex)) return;
            if (!checkVariationValuePresent(variationData, "rotation", variationIndex)) return;
            if (!checkVariationValuePresent(variationData, "scale", variationIndex)) return;


            //check variation settings are valid
            if (!checkVariationNum(variationData, &newFlameConfig.variations[variationIndex], variationIndex)) return;

            if (!checkVariationValueFloatArray(variationData, "color", 0.0f, 1.0f, &tempArray, variationIndex) || tempArray.size() != 3) return;
            newFlameConfig.colors[variationIndex * 3 + 0] = tempArray[0];
            newFlameConfig.colors[variationIndex * 3 + 1] = tempArray[1];
            newFlameConfig.colors[variationIndex * 3 + 2] = tempArray[2];

            if (!checkVariationValueFloat(variationData, "weight", 0.0f, FLT_MAX, &newFlameConfig.weights[variationIndex], variationIndex)) return;

            tempArray.clear();
            if (!checkVariationValueFloatArray(variationData, "translation", -FLT_MAX, FLT_MAX, &tempArray, variationIndex) || tempArray.size() != 2) return;
            newFlameConfig.translations[variationIndex].x = tempArray[0];
            newFlameConfig.translations[variationIndex].y = tempArray[1];

            if (!checkVariationValueFloat(variationData, "rotation", -FLT_MAX, FLT_MAX, &newFlameConfig.rotations[variationIndex], variationIndex)) return;

            tempArray.clear();
            if (!checkVariationValueFloatArray(variationData, "scale", -FLT_MAX, FLT_MAX, &tempArray, variationIndex) || tempArray.size() != 2) return;
            newFlameConfig.scales[variationIndex].x = tempArray[0];
            newFlameConfig.scales[variationIndex].y = tempArray[1];

            newFlameConfig.numVariations++;
        }


        //check camera settings are valid
        glm::vec2 newCamPos;
        float newCamAngle, newCamZoom;

        tempArray.clear();
        if (!checkCamValueFloatArray(inputData, "position", -FLT_MAX, FLT_MAX, &tempArray)) return;
        newCamPos.x = tempArray[0];
        newCamPos.y = tempArray[1];

        if (!checkCamValueFloat(inputData, "angle", -FLT_MAX, FLT_MAX, &newCamAngle)) return;
        if (!checkCamValueFloat(inputData, "zoom", -FLT_MAX, FLT_MAX, &newCamZoom)) return;

        
        //check colour processing values are valid
        float newBrightness, newIntensity, newGamma;
        if (!checkColorValueFloat(inputData, "brightness", MIN_BRIGHTNESS, MAX_BRIGHTNESS, &newBrightness)) return;
        if (!checkColorValueFloat(inputData, "intensity", MIN_INTENSITY, MAX_INTENSITY, &newIntensity)) return;
        if (!checkColorValueFloat(inputData, "gamma", MIN_GAMMA, MAX_GAMMA, &newGamma)) return;


        //actually apply the values
		loadFlameConfig(newFlameConfig);

        cam.reset();
        cam.updatePosition(newCamPos);
        cam.updateRotation(newCamAngle);
        cam.updateZoom(newCamZoom);

        setBrightness(newBrightness);
        setIntensity(newIntensity);
        setGamma(newGamma);
	}

	void createGUI(const float frameDuration)
	{
		float UI_SAMPLE_SETTINGS_WIDTH = 12.5f * ImGui::GetFontSize();
		float UI_VARIATION_SETTINGS_WIDTH = 18.0f * ImGui::GetFontSize();

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSizeConstraints(ImVec2(10, 10), ImVec2(previewTexWidth, previewTexHeight));
		ImGui::SetNextWindowSize(ImVec2(0, previewTexHeight), ImGuiCond_Once);
		ImGui::SetNextWindowBgAlpha(0.3f);
		ImGui::Begin("Menu", NULL);

		ImGui::Text("%d fps", (int)(1.0f / frameDuration));

		ImGui::SeparatorText("Controls");
		ImGui::Columns(2);

		ImGui::Text("Move camera");
		ImGui::Text("Rotate camera");
		ImGui::Text("Zoom camera");
		ImGui::Text("Move fractal");
		ImGui::Text("Zoom fractal");
		ImGui::Text("Rotate fractal");
		ImGui::Text("Camera speed");
		ImGui::Text("Toggle GUI");

		ImGui::NextColumn();

		ImGui::Text("W S A D");
		ImGui::Text("Q E");
		ImGui::Text("R F");
		ImGui::Text("Left click");
		ImGui::Text("Shift + left click");
		ImGui::Text("Ctrl + left click");
		ImGui::Text("X C");
		ImGui::Text("H");

		ImGui::Columns(1);

		ImGui::TextLinkOpenURL("More Info on GitHub", "https://github.com/alexf13e/fractal-flame#usage");
		ImGui::Spacing();

		ImGui::SeparatorText("Settings");

		ImGui::PushItemWidth(UI_SAMPLE_SETTINGS_WIDTH);

		if (ImGui::Button(paused ? "Resume" : "Pause", ImVec2(UI_SAMPLE_SETTINGS_WIDTH, 0)))
		{
			togglePause();
		}

		if (ImGui::Button("Clear image", ImVec2(UI_SAMPLE_SETTINGS_WIDTH, 0)))
		{
			clearSingleFrame = true;
		}

		ImGui::Checkbox("Clear every frame", &clearEveryFrame);

		if (ImGui::Checkbox("Faster plotting (less accurate)", &plotWithoutAtomic))
		{
			clearSingleFrame = true;
			if (paused) togglePause();
		}

		ImGui::Spacing();

		int temp = numSampleThreads;
		if (ImGui::DragInt("Sample threads", &temp, 4.0f, 1, 16384, "%d", ImGuiSliderFlags_ClampOnInput))
		{
			numSampleThreads = temp;
		}

		temp = initialIterations;
		if (ImGui::InputInt("Initial iterations", &temp, 1, 10))
		{
			setInitialIterations(glm::max(temp, 0));
		}

		temp = drawingIterations;
		if (ImGui::InputInt("Drawing iterations", &temp, 100, 1000))
		{
			setDrawingIterations(glm::max(temp, 0));
		}

		temp = maxPreviewFrames;
		if (ImGui::InputInt("Max preview frames", &temp, 10, 100))
		{
			setMaxPreviewFrames(glm::max(temp, 0));
		}

		if (maxPreviewFrames > 0) ImGui::Text("Preview frames rendered: %d/%d", frameNum, maxPreviewFrames);
		else ImGui::Text("Frames rendered: %d", frameNum);

		ImGui::Spacing();

		ImGui::SeparatorText("Camera");

		if (ImGui::Button("Reset camera", ImVec2(UI_SAMPLE_SETTINGS_WIDTH, 0)))
		{
			resetCam();
		}

		glm::vec2 camPos = cam.position;
		if (ImGui::DragFloat2("Camera position", &camPos.x, 2.0f / previewTexWidth * cam.view.x))
		{
			glm::vec2 delta = glm::vec2(camPos - cam.position);
			delta = glm::rotateZ(glm::vec3(delta, 0.0f), cam.angle);

			if (delta != glm::vec2(0.0f))
			{
				cam.updatePosition(delta);
				if (paused) clearOnUnpause = true;
				else clearSingleFrame = true;
			}
		}

		float camZoom = cam.zoom;
		if (ImGui::DragFloat("Camera zoom", &camZoom, 0.025f, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM, "%.3f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_ClampOnInput))
		{
			float delta = camZoom / cam.zoom;

			if (delta != 1.0f)
			{
				cam.updateZoom(delta);
				if (paused) clearOnUnpause = true;
				else clearSingleFrame = true;
			}
		}

		float camAngle = glm::degrees(cam.angle);
		if (ImGui::DragFloat("Camera angle", &camAngle, 0.75f))
		{
			camAngle = glm::mod(glm::radians(camAngle), glm::two_pi<float>());

			float delta = camAngle - cam.angle;
			if (delta != 0.0f)
			{
				cam.updateRotation(delta);
				if (paused) clearOnUnpause = true;
				else clearSingleFrame = true;
			}
		}

		if (ImGui::BeginCombo("Guidelines", GUIDELINE_NAMES[guideLineNum]))
		{
			for (uint32_t j = 0; j < NUM_GUIDELINE_NAMES; j++)
			{
				bool is_selected = guideLineNum == j;
				if (ImGui::Selectable(GUIDELINE_NAMES[j], is_selected))
				{
					guideLineNum = j;
				}
				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();

		ImGui::SeparatorText("Color processing");

		if (ImGui::DragFloat("Brightness", &brightness, 0.005f, MIN_BRIGHTNESS, MAX_BRIGHTNESS, "%.3f", ImGuiSliderFlags_ClampOnInput))
		{
			setBrightness(brightness);
		}

		if (ImGui::DragFloat("Intensity", &intensity, 0.005f, MIN_INTENSITY, MAX_INTENSITY, "%.3f", ImGuiSliderFlags_ClampOnInput))
		{
			setIntensity(intensity);
		}

		if (ImGui::DragFloat("Gamma", &gamma, 0.01f, MIN_GAMMA, MAX_GAMMA, "%.3f", ImGuiSliderFlags_ClampOnInput))
		{
			setGamma(gamma);
		}
		
		ImGui::Spacing();

		ImGui::SeparatorText("Render");

		int res[2] = { (int)renderTexWidth, (int)renderTexHeight };

		if (ImGui::Checkbox("Match preview", &renderMatchPreview) && renderMatchPreview)
		{
			renderTexWidth = previewTexWidth;
			renderTexHeight = previewTexHeight;
			numRenderFrames = frameNum;
		}

		if (renderMatchPreview) ImGui::BeginDisabled();
		if (ImGui::InputInt2("Render resolution", res))
		{
			res[0] = glm::clamp(res[0], MIN_IMAGE_SIZE, MAX_IMAGE_SIZE);
			res[1] = glm::clamp(res[1], MIN_IMAGE_SIZE, MAX_IMAGE_SIZE);
			renderTexWidth = res[0];
			renderTexHeight = res[1];
		}

		int n = numRenderFrames;
		if (ImGui::InputInt("Number of frames", &n, 1, 10))
		{
			if (n < 0) n = 0;
			numRenderFrames = n;
		}
		if (renderMatchPreview) ImGui::EndDisabled();

		ImGui::PopItemWidth();

		if (ImGui::Button("Save as image", ImVec2(UI_SAMPLE_SETTINGS_WIDTH, 0)))
		{
			renderNextFrame = 2; //to allow imgui to display info that saving has started, delay actually starting until next frame
			appendInfo("Saving image...");
		}

		if (ImGui::Button("Save unprocessed data", ImVec2(UI_SAMPLE_SETTINGS_WIDTH, 0)))
		{
			saveUnprocessedDataNextFrame = 2; //to allow imgui to display info that saving has started, delay actually starting until next frame
			appendInfo("Saving unprocessed data...");
		}

		ImGui::Spacing();

		ImGui::SeparatorText("Info");
		std::string infoCombined = "";
		for (const std::string& i : info)
		{
			infoCombined += i + "\n";
		}
		ImGui::TextWrapped(infoCombined.c_str());
		ImGui::End();


		ImGui::SetNextWindowPos(ImVec2(previewTexWidth, 0), ImGuiCond_None, ImVec2(1, 0));
		ImGui::SetNextWindowSizeConstraints(ImVec2(10, 10), ImVec2(previewTexWidth, previewTexHeight));
		ImGui::SetNextWindowSize(ImVec2(0, previewTexHeight), ImGuiCond_Once);
		ImGui::SetNextWindowBgAlpha(0.3f);
		ImGui::Begin("Variations", NULL);

		if (currentFlame.numVariations > 0)
		{
			if (ImGui::Button("Save flame config"))
			{
				saveFlameFile();
			}

			ImGui::SameLine();

			if (ImGui::Button("Load flame config"))
			{
				loadFlameFile();
			}

			uint32_t numPreviousFlames = previousFlames.size(); //size can change before calling EndDisabled()
			if (numPreviousFlames == 0) ImGui::BeginDisabled();
			if (ImGui::Button("Previous flame"))
			{
				loadFlameConfig(previousFlames.top(), false);
				previousFlames.pop();
			}
			if (numPreviousFlames == 0) ImGui::EndDisabled();

			ImGui::SeparatorText("Randomise");

			const float BUTTON_WIDTH = 7.5f * ImGui::GetFontSize();

			if (ImGui::Button("Variations", ImVec2(BUTTON_WIDTH, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationNum(i, VALID_VARIATIONS[randomVariationNum()]);
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("colors", ImVec2(BUTTON_WIDTH, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationcolor(i, randomOKLChtoRGB());
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Weights", ImVec2(BUTTON_WIDTH, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationWeight(i, randomFloat());
				}
			}

			//new line

			if (ImGui::Button("Rotations", ImVec2(BUTTON_WIDTH, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationRotation(i, randomFloat() * glm::two_pi<float>());
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Translations", ImVec2(BUTTON_WIDTH, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationTranslation(i, glm::vec2(randomFloat() * 2.0f - 1.0f, randomFloat() * 2.0f - 1.0f));
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Scales", ImVec2(BUTTON_WIDTH, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationScale(i, glm::vec2(randomFloat() * 4.0f - 2.0f, randomFloat() * 4.0f - 2.0f));
				}
			}

			if (ImGui::Button("All", ImVec2(BUTTON_WIDTH * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationNum(i, VALID_VARIATIONS[randomVariationNum()]);
					glm::vec3 rgb = randomOKLChtoRGB();
					setVariationcolor(i, rgb);
					setVariationWeight(i, randomFloat());
					setVariationTranslation(i, glm::vec2(randomFloat(), randomFloat()));
					setVariationRotation(i, randomFloat() * glm::two_pi<float>());
					setVariationScale(i, glm::vec2(randomFloat() * 4.0f - 2.0f, randomFloat() * 4.0f - 2.0f));
				}
			}
		}

		ImGui::Spacing();

		if (currentFlame.numVariations < MAX_VARIATIONS)
		{
			if (ImGui::Button("Add variation"))
			{
				addDefaultVariation();
			}
		}

		ImGui::Separator();

		ImGui::BeginChild("VariationWindow");
		ImGui::PushItemWidth(UI_VARIATION_SETTINGS_WIDTH);
		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			ImGui::PushID(i);

			if (ImGui::BeginCombo("Variation", (std::to_string(currentFlame.variations[i]) + " - " + VARIATION_NAMES[currentFlame.variations[i]]).c_str()))
			{
				for (uint32_t j = 0; j < NUM_VALID_VARIATIONS; j++)
				{
					bool is_selected = currentFlame.variations[i] == VALID_VARIATIONS[j];
					if (ImGui::Selectable((std::to_string(VALID_VARIATIONS[j]) + " - " + VARIATION_NAMES[VALID_VARIATIONS[j]]).c_str(), is_selected))
					{
						setVariationNum(i, VALID_VARIATIONS[j]);
					}
					if (is_selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::ColorEdit3("color", &currentFlame.colors[i * 3]))
			{
				glm::vec3 col = { currentFlame.colors[i * 3], currentFlame.colors[i * 3 + 1], currentFlame.colors[i * 3 + 2] };
				setVariationcolor(i, col);
			}

			if (ImGui::SliderFloat("Weight", &currentFlame.weights[i], 0.0f, 1.0f))
			{
				setVariationWeight(i, currentFlame.weights[i]);
			}

			float r = glm::degrees(currentFlame.rotations[i]);
			if (ImGui::DragFloat("Rotation", &r, 1.0f))
			{
				setVariationRotation(i, glm::radians(r));
			}

			if (ImGui::DragFloat2("Translation", &currentFlame.translations[i].x, 0.01f))
			{
				setVariationTranslation(i, currentFlame.translations[i]);
			}

			if (ImGui::DragFloat2("Scale", &currentFlame.scales[i].x, 0.01f))
			{
				setVariationScale(i, currentFlame.scales[i]);
			}

			if (ImGui::Button("Remove"))
			{
				removeVariation(i);
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::PopID();
		}
		ImGui::PopItemWidth();
		ImGui::EndChild();

		ImGui::End();
	}

	bool init(uint32_t tw, uint32_t th)
	{
		if (!shFullScreenTri.init("./shaders/fullScreenTri.vert", "./shaders/ifs.frag")) return false;
		if (!shLines.init("./shaders/line.vert", "./shaders/line.frag")) return false;

		glGenVertexArrays(1, &vao_fullScreenTri);

		glGenVertexArrays(1, &vao_lines);
		glBindVertexArray(vao_lines);
		glGenBuffers(1, &vbo_guideLines2x2);
		glGenBuffers(1, &vbo_guideLines3x3);
		glGenBuffers(1, &vbo_mouseLine);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_guideLines2x2);

		std::vector<float> lines = {
			-1.0f, 0.0f,
			1.0f, 0.0f,
			0.0f, -1.0f,
			0.0f, 1.0f
		};

		glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_guideLines3x3);

		float oneThird = 1.0f / 3.0f;
		lines = {
			-1.0f, -oneThird,
			1.0f, -oneThird,
			-1.0f, oneThird,
			1.0f, oneThird,
			-oneThird, -1.0f,
			-oneThird, 1.0f,
			oneThird, -1.0f,
			oneThird, 1.0f
		};

		glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		glEnable(GL_BLEND);


		for (uint32_t i = 0; i < MAX_VARIATIONS; i++)
		{
			currentFlame.variations[i] = 0;
			currentFlame.colors[i * 3 + 0] = 0.0f;
			currentFlame.colors[i * 3 + 1] = 0.0f;
			currentFlame.colors[i * 3 + 2] = 0.0f;
			currentFlame.weights[i] = 0.0f;
			currentFlame.transforms[i * 6 + 0] = 0.0f;
			currentFlame.transforms[i * 6 + 1] = 0.0f;
			currentFlame.transforms[i * 6 + 2] = 0.0f;
			currentFlame.transforms[i * 6 + 3] = 0.0f;
			currentFlame.transforms[i * 6 + 4] = 0.0f;
			currentFlame.transforms[i * 6 + 5] = 0.0f;

			currentFlame.translations[i] = glm::vec2(0.0f);
			currentFlame.rotations[i] = 0.0f;
			currentFlame.scales[i] = glm::vec2(0.0f);
		}

		CLManager::createBuffer<uint32_t>(b_variations, MAX_VARIATIONS, currentFlame.variations);
		CLManager::createBuffer<float>(b_colors, MAX_VARIATIONS * 3, currentFlame.colors);
		CLManager::createBuffer<float>(b_weights, MAX_VARIATIONS, currentFlame.weights);
		CLManager::createBuffer<float>(b_transforms, MAX_VARIATIONS * 6, currentFlame.transforms);

		CLManager::createKernel(k_produceSamples);
		CLManager::createKernel(k_postProcess);
		CLManager::createKernel(k_floatToByte);

		CLManager::setKernelParamLocal<uint32_t>(k_produceSamples, 14, MAX_VARIATIONS);
		CLManager::setKernelParamLocal<float>(k_produceSamples, 15, MAX_VARIATIONS * 3);
		CLManager::setKernelParamLocal<float>(k_produceSamples, 16, MAX_VARIATIONS);
		CLManager::setKernelParamLocal<float>(k_produceSamples, 17, MAX_VARIATIONS * 6);

		cam.init(tw, th, glm::vec2(0.0f));
		lastRenderedCamPos = cam.position;
		lastRenderedCamZoom = cam.zoom;
		lastRenderedCamAngle = cam.angle;
		glm::mat4 matPausedCamTransform = glm::mat4(1.0f);

		guideLineNum = 0;

		setPreviewTexSize(tw, th); //preview texture created here

		//default values for options
		renderMatchPreview = true;
		if (renderMatchPreview)
		{
			renderTexWidth = previewTexWidth;
			renderTexHeight = previewTexHeight;

		}
		else
		{
			renderTexWidth = 3840;
			renderTexHeight = 2160;
		}

		numSampleThreads = CLManager::kernelLocalRanges[k_produceSamples].get()[0]
			* CLManager::device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
		
		setMaxPreviewFrames(100);
		numRenderFrames = 100;
		setInitialIterations(20);
		setDrawingIterations(1000);

		setBrightness(DEFAULT_BRIGHTNESS);
		setIntensity(DEFAULT_INTENSITY);
		setGamma(DEFAULT_GAMMA);

		paused = false;
		clearEveryFrame = false;
		clearSingleFrame = false;
		plotWithoutAtomic = true;
		wantsPostProcess = false;
		drawMouseLine = false;
		clearOnUnpause = false;

		//start the program with 3 random variations
		addRandomVariation();
		addRandomVariation();
		addRandomVariation();

		return true;
	}

	void updateKernelParams()
	{
		CLManager::setKernelRange(k_produceSamples, numSampleThreads);
		CLManager::setKernelParamBuffer(k_produceSamples, 0, { b_previewTexture, b_variations, b_colors, b_weights, b_transforms });
		CLManager::setKernelParamValue(k_produceSamples, 5, currentFlame.numVariations);
		CLManager::setKernelParamValue(k_produceSamples, 6, initialIterations);
		CLManager::setKernelParamValue(k_produceSamples, 7, drawingIterations);
		CLManager::setKernelParamValue(k_produceSamples, 8, cam.getMatViewCL());
		CLManager::setKernelParamValue(k_produceSamples, 9, previewTexWidth);
		CLManager::setKernelParamValue(k_produceSamples, 10, previewTexHeight);
		CLManager::setKernelParamValue<uint8_t>(k_produceSamples, 11, plotWithoutAtomic);
		CLManager::setKernelParamValue(k_produceSamples, 12, frameNum);
		CLManager::setKernelParamValue(k_produceSamples, 13, numSampleThreads);

		uint32_t numPixels = previewTexWidth * previewTexHeight;
		CLManager::setKernelRange(k_postProcess, numPixels);
		CLManager::setKernelParamBuffer(k_postProcess, 0, { b_previewTexture });
		CLManager::setKernelParamGLBuffer(k_postProcess, 1, { glb_previewTextureProcessed });
		CLManager::setKernelParamValue(k_postProcess, 2, brightness);
		CLManager::setKernelParamValue(k_postProcess, 3, intensity);
		CLManager::setKernelParamValue(k_postProcess, 4, gamma);
		CLManager::setKernelParamValue(k_postProcess, 5, numPixels);
	}

	void update()
	{
		if (renderNextFrame == 1)
		{
			saveProcessedImage();
		}

		if (saveUnprocessedDataNextFrame == 1)
		{
			saveUnprocessedData();
		}

		if (renderNextFrame > 0) renderNextFrame--;
		if (saveUnprocessedDataNextFrame > 0) saveUnprocessedDataNextFrame--;

		if (!paused && clearEveryFrame)
		{
			clearSamples();
			clearSingleFrame = false;
		}

		if (clearSingleFrame)
		{
			clearSamples();
			clearSingleFrame = false;
		}

		updateKernelParams();

		if (!paused && currentFlame.numVariations > 0 && !getMaxPreviewFramesReached())
		{
			CLManager::runKernel(k_produceSamples);
			frameNum++;
			if (renderMatchPreview) numRenderFrames = frameNum;

			wantsPostProcess = true;
		}

		if (wantsPostProcess)
		{
			acquireGLObjects();
			CLManager::runKernel(k_postProcess);
			releaseGLObjects();
			wantsPostProcess = false;
		}
	}

	void clearSamples()
	{
		//clear the preview buffer and start from 0 samples
		CLManager::fillBuffer(b_previewTexture, previewTexWidth * previewTexHeight * 4, 0);
		frameNum = 0;
		if (renderMatchPreview) numRenderFrames = frameNum;
		wantsPostProcess = true;
	}

	void draw()
	{
		//draw the preview buffer to the screen
		glUseProgram(shFullScreenTri.getID());
		
		glBindVertexArray(vao_fullScreenTri);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		//draw guidelines if requested
		if (guideLineNum != 0)
		{
			glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
			glUseProgram(shLines.getID());
			glBindVertexArray(vao_lines);
			if (guideLineNum == 1)
			{
				glBindBuffer(GL_ARRAY_BUFFER, vbo_guideLines2x2);
				GLint attribLocation = glGetAttribLocation(shLines.getID(), "vs_position");
				glEnableVertexAttribArray(attribLocation);
				glVertexAttribPointer(attribLocation, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
			}
			if (guideLineNum == 2)
			{
				glBindBuffer(GL_ARRAY_BUFFER, vbo_guideLines3x3);
				GLint attribLocation = glGetAttribLocation(shLines.getID(), "vs_position");
				glEnableVertexAttribArray(attribLocation);
				glVertexAttribPointer(attribLocation, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
			}
			glDrawArrays(GL_LINES, 0, guideLineNum * 4);
			glBlendFunc(GL_ONE, GL_ZERO);
		}

		//draw mouse line if requested
		if (drawMouseLine)
		{
			glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
			glUseProgram(shLines.getID());
			glBindVertexArray(vao_lines);
			glBindBuffer(GL_ARRAY_BUFFER, vbo_mouseLine);
			GLint attribLocation = glGetAttribLocation(shLines.getID(), "vs_position");
			glEnableVertexAttribArray(attribLocation);
			glVertexAttribPointer(attribLocation, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
			glDrawArrays(GL_LINES, 0, 4);
			glBlendFunc(GL_ONE, GL_ZERO);

			drawMouseLine = false;
		}

		glBindVertexArray(0);
		glUseProgram(0);

	}

	bool checkImageCanRender()
	{
		uint32_t numPixels = renderTexWidth * renderTexHeight;
		uint64_t numBytes = numPixels * 4 * sizeof(float);
		uint64_t maxAllocationSize = CLManager::device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
		if (numBytes > maxAllocationSize)
		{
			std::cout << "Failed to allocate GPU memory for image" << std::endl;
			std::cout << "required: " + std::to_string(numBytes / 1024 / 1024) + "MB, max allocation: " +
				std::to_string(maxAllocationSize / 1024 / 1024) + "MB" << std::endl;
				
			appendInfo("Failed to allocate GPU memory for image");

			uint32_t maxSquareRes = glm::sqrt(maxAllocationSize / sizeof(float) / 4);
			appendInfo("Try a lower resolution - the largest square image would be " + std::to_string(maxSquareRes) + "x" + std::to_string(maxSquareRes));
			return false;
		}

		return true;
	}

	std::string getStringTimestamp()
	{
		time_t rawTime;
		tm* timeInfo;
		char timeBuffer[32]; //probably only need 19 for YYYY-MM-DD_HH-MM-SS

		time(&rawTime);
		timeInfo = localtime(&rawTime);
		strftime(timeBuffer, 80, "%F_%H-%M-%S", timeInfo);

		return std::string(timeBuffer);
	}

	void saveProcessedImage()
	{
		//render to an image file

		bool doNewRender = !renderMatchPreview || clearOnUnpause;
		if (doNewRender && !checkImageCanRender()) return;

		//set save path
		std::string fileName = getStringTimestamp();
		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			fileName += "_" + std::to_string(currentFlame.variations[i]);
		}

		std::vector<nfdu8filteritem_t> filters = { { "PNG Image", "png" } };
		std::string renderOutputPath = FileDialog::saveDialog(fileName, filters);
		if (renderOutputPath == "")
		{
			//pressed cancel, so don't render
			appendInfo("Cancelled");
			return;
		}

		uint32_t numPixels = renderTexWidth * renderTexHeight;
		CLManager::createBuffer<uint8_t>(b_renderTextureBytes, numPixels * 4);

		if (doNewRender)
		{
			CLManager::createBuffer<float>(b_renderTexture, numPixels * 4);

			std::cout << "Rendering..." << std::endl;

			//produce the samples on the texture
			CLManager::setKernelRange(k_produceSamples, numSampleThreads * numRenderFrames);
			CLManager::setKernelParamBuffer(k_produceSamples, 0, { b_renderTexture });
			cam.setAspectRatio(renderTexWidth, renderTexHeight);
			CLManager::setKernelParamValue(k_produceSamples, 8, cam.getMatViewCL());
			CLManager::setKernelParamValue(k_produceSamples, 9, renderTexWidth);
			CLManager::setKernelParamValue(k_produceSamples, 10, renderTexHeight);
			CLManager::setKernelParamValue(k_produceSamples, 11, plotWithoutAtomic);
			CLManager::setKernelParamValue(k_produceSamples, 12, 0);
			CLManager::setKernelParamValue(k_produceSamples, 13, numSampleThreads * numRenderFrames);
			CLManager::runKernel(k_produceSamples);

			std::cout << "Applying post process..." << std::endl;

			//apply brightness and gamma
			CLManager::setKernelRange(k_postProcess, numPixels);
			CLManager::setKernelParamBuffer(k_postProcess, 0, { b_renderTexture, b_renderTexture });
			CLManager::setKernelParamValue(k_postProcess, 5, numPixels);
			CLManager::runKernel(k_postProcess);

			//convert from float to byte for writing to file
			CLManager::setKernelRange(k_floatToByte, numPixels);
			CLManager::setKernelParamBuffer(k_floatToByte, 0, { b_renderTexture, b_renderTextureBytes });
			CLManager::setKernelParamValue(k_floatToByte, 2, numPixels);
			CLManager::runKernel(k_floatToByte);

			CLManager::deleteBuffer(b_renderTexture);
			cam.setAspectRatio(previewTexWidth, previewTexHeight);
		}
		else
		{
			std::cout << "render size and samples match preview, so skipping re-render" << std::endl;

			acquireGLObjects();
			CLManager::setKernelRange(k_floatToByte, numPixels);
			CLManager::setKernelParamGLBuffer(k_floatToByte, 0, { glb_previewTextureProcessed });
			CLManager::setKernelParamBuffer(k_floatToByte, 1, { b_renderTextureBytes });
			CLManager::setKernelParamValue(k_floatToByte, 2, numPixels);
			CLManager::runKernel(k_floatToByte);
			releaseGLObjects();
		}
		
		std::cout << "Saving to " << renderOutputPath << std::endl;

		//save the texture to an image
		uint8_t* texture = new uint8_t[numPixels * 4];
		CLManager::readBuffer(b_renderTextureBytes, numPixels * 4, texture);
		stbi_write_png_compression_level = 1;
		stbi_flip_vertically_on_write(1);
		stbi_write_png(renderOutputPath.c_str(), renderTexWidth, renderTexHeight, 4, texture, renderTexWidth * 4 * sizeof(uint8_t));
		delete[] texture;
		CLManager::deleteBuffer(b_renderTextureBytes);

		std::cout << "Render complete" << std::endl;
		appendInfo("Image finished, saved to " + renderOutputPath);
	}

	void saveUnprocessedData()
	{
		//save array of unprocessed pixel values
		
		bool doNewRender = !renderMatchPreview || clearOnUnpause;
		if (doNewRender && !checkImageCanRender()) return;

		std::string fileName = getStringTimestamp();
		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			fileName += "_" + std::to_string(currentFlame.variations[i]);
		}

		std::vector<nfdu8filteritem_t> filters = { { "Raw data", "data" } };
		std::string renderOutputPath = FileDialog::saveDialog(fileName, filters);
		if (renderOutputPath == "")
		{
			//pressed cancel, so don't render
			appendInfo("Cancelled");
			return;
		}

		uint32_t numPixels = renderTexWidth * renderTexHeight;
		float* data = new float[numPixels * 4];

		if (doNewRender)
		{
			CLManager::createBuffer<float>(b_renderTexture, numPixels * 4);

			std::cout << "Rendering..." << std::endl;

			//produce the samples on the texture
			CLManager::setKernelRange(k_produceSamples, numSampleThreads * numRenderFrames);
			CLManager::setKernelParamBuffer(k_produceSamples, 0, { b_renderTexture });
			cam.setAspectRatio(renderTexWidth, renderTexHeight);
			CLManager::setKernelParamValue(k_produceSamples, 8, cam.getMatViewCL());
			CLManager::setKernelParamValue(k_produceSamples, 9, renderTexWidth);
			CLManager::setKernelParamValue(k_produceSamples, 10, renderTexHeight);
			CLManager::setKernelParamValue(k_produceSamples, 11, plotWithoutAtomic);
			CLManager::setKernelParamValue(k_produceSamples, 12, 0);
			CLManager::setKernelParamValue(k_produceSamples, 13, numSampleThreads * numRenderFrames);
			CLManager::runKernel(k_produceSamples);

			CLManager::readBuffer(b_renderTexture, numPixels * 4, data);
			CLManager::deleteBuffer(b_renderTexture);
		}
		else
		{
			CLManager::readBuffer(b_previewTexture, numPixels * 4, data);
		}

		std::ofstream f(renderOutputPath, std::ios::out | std::ios::binary);
    
		//first 8 bytes are image width and height
		uint8_t b0 = (renderTexWidth & 0xff000000) >> 24;
		uint8_t b1 = (renderTexWidth & 0x00ff0000) >> 16;
		uint8_t b2 = (renderTexWidth & 0x0000ff00) >> 8;
		uint8_t b3 = (renderTexWidth & 0x000000ff);
		f << b0 << b1 << b2 << b3;

		b0 = (renderTexHeight & 0xff000000) >> 24;
		b1 = (renderTexHeight & 0x00ff0000) >> 16;
		b2 = (renderTexHeight & 0x0000ff00) >> 8;
		b3 = (renderTexHeight & 0x000000ff);
		f << b0 << b1 << b2 << b3;

		f.write(reinterpret_cast<char*>(data), numPixels * 4 * sizeof(float));
		if (f.bad())
		{
			std::cout << "error saving file: " << strerror(errno) << std::endl;
			appendInfo("error saving file: " + std::string(strerror(errno)));
		}
		else
		{
			std::cout << "Saving unprocessed data complete" << std::endl;
			appendInfo("Saving finished, saved to " + renderOutputPath);
		}

		f.close();
		
		delete[] data;
	}

	float randomFloat()
	{
		return (float)rand() / RAND_MAX;
	}

	uint32_t randomVariationNum()
	{
		//don't want variation 0
		return (uint32_t)(1 + randomFloat() * (NUM_VALID_VARIATIONS - 1));
	}

	glm::vec3 randomOKLChtoRGB()
	{
		//https://bottosson.github.io/posts/oklab/

		//generate a random "sensible" color in LCh space
		float L = 0.3f + randomFloat() * 0.5f;
		float C = randomFloat() * 0.5f;
		float h = randomFloat() * 2.0f * PI;

		float a = C * cos(h);
		float b = C * sin(h);
		float okLAB[3] = { L, a, b };

		float l_ = okLAB[0] + 0.3963377774f * okLAB[1] + 0.2158037573f * okLAB[2];
		float m_ = okLAB[0] - 0.1055613458f * okLAB[1] - 0.0638541728f * okLAB[2];
		float s_ = okLAB[0] - 0.0894841775f * okLAB[1] - 1.2914855480f * okLAB[2];

		float l = l_ * l_ * l_;
		float m = m_ * m_ * m_;
		float s = s_ * s_ * s_;

		glm::vec3 rgb = {
			+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
			-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
			-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
		};

		rgb = glm::clamp(rgb, 0.0f, 1.0f);
		return rgb;
	}

	void appendInfo(const std::string& s)
	{
		while (info.size() >= infoLength)
		{
			info.pop_front();
		}

		info.push_back(s);
	}
}
