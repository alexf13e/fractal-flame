
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
		uint32_t guideLineNum;

		uint32_t previewTexWidth, previewTexHeight;
		uint32_t renderTexWidth, renderTexHeight;
		bool renderTexSizeMatchPreview;
		bool renderFrameNumMatchPreview;
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
		constexpr float MIN_BRIGHTNESS = 0.0f;
		constexpr float MAX_BRIGHTNESS = 2.0f;
		constexpr float MIN_INTENSITY = 0.0f;
		constexpr float MAX_INTENSITY = 1.0f;
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
		if (paused) return;

		cam.updatePosition(deltaPos);
		cam.updateZoom(deltaZoom);
		cam.updateRotation(deltaAngle);
		clearSingleFrame = true;
	}

	void updateCamPositionMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen)
	{
		if (paused) return;

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
			clearSingleFrame = true;
		}
	}

	void updateCamZoomMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen)
	{
		if (paused) return;

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
			clearSingleFrame = true;
		}
	}

	void updateCamRotationMouse(const glm::vec2& currentMousePosScreen, const glm::vec2& prevMousePosScreen)
	{
		if (paused) return;

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
			clearSingleFrame = true;
		}
	}

	void resetCam()
	{
		cam.reset();
		clearSingleFrame = true;
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
		if (renderTexSizeMatchPreview)
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
	}

	void setInitialIterations(uint32_t n)
	{
		//number of iterations which will run on sample points before their positions are drawn to the buffer
		initialIterations = n;
		clearSingleFrame = true;
	}

	void setDrawingIterations(uint32_t n)
	{
		//number of iterations after top of initialIterations, where the sample position at each iteration WILL be drawn
		drawingIterations = n;
		clearSingleFrame = true;
	}

	void setBrightness(float b)
	{
		//brightness is multiplied with pixel value before gamma
		brightness = b;
		wantsPostProcess = true;
	}

	void setIntensity(float v)
	{
		//intensity blends between using
		//	rgba = pow(rgba, 1 / gamma)
		//	rgba *= pow(a, 1 / gamma)
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

			clearSingleFrame = true;
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
	}

	void setVariationcolor(uint32_t index, glm::vec3 rgb)
	{
		if (index >= currentFlame.numVariations) return;

		currentFlame.colors[index * 3 + 0] = rgb.x;
		currentFlame.colors[index * 3 + 1] = rgb.y;
		currentFlame.colors[index * 3 + 2] = rgb.z;

		CLManager::writeBuffer(b_colors, 3, &currentFlame.colors[index * 3], index * 3);
		clearSingleFrame = true;
	}

	void setVariationWeight(uint32_t index, float w)
	{
		if (index >= currentFlame.numVariations) return;

		currentFlame.weights[index] = w;
		CLManager::writeBuffer(b_weights, 1, &currentFlame.weights[index], index);
		clearSingleFrame = true;
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
		paused = false;
	}

	void saveFlameFile()
	{
		std::string fileName = "config";
		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			fileName += "_" + std::to_string(currentFlame.variations[i]);
		}

		std::vector<nfdu8filteritem_t> filters = { { "Flame config", "flame" } };
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

		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			fileStream <<
				std::to_string(currentFlame.variations[i]) << "," <<
				std::to_string(currentFlame.colors[i * 3]) << "," <<
				std::to_string(currentFlame.colors[i * 3 + 1]) << "," <<
				std::to_string(currentFlame.colors[i * 3 + 2]) << "," <<
				std::to_string(currentFlame.weights[i]) << ", " <<
				std::to_string(currentFlame.translations[i].x) << ", " <<
				std::to_string(currentFlame.translations[i].y) << ", " <<
				std::to_string(currentFlame.rotations[i]) << ", " <<
				std::to_string(currentFlame.scales[i].x) << ", " <<
				std::to_string(currentFlame.scales[i].y) <<
				std::endl;
		}

		fileStream << std::to_string(cam.position.x) << ", " << std::to_string(cam.position.y) << ", " <<
			std::to_string(cam.angle) << ", " << std::to_string(cam.zoom) << std::endl;

		fileStream << std::to_string(brightness) << ", " << std::to_string(intensity) << ", " << std::to_string(gamma)
			<< std::endl;
	}

	void loadFlameFile()
	{
		auto printErrInvalidData = []() {
			appendInfo("Flame config file contains invalid data, loading cancelled");
		};

		auto checkVarNum = [&](const std::string& val, uint32_t* dest) {
			int varNum;
			try
			{
				varNum = std::stoi(val);
			}
			catch (std::invalid_argument e)
			{
				printErrInvalidData();
				return false;
			}

			bool varNumValid = false;
			for (uint32_t validVarNum : VALID_VARIATIONS)
			{
				if (varNum == validVarNum)
				{
					varNumValid = true;
					break;
				}
			}

			if (!varNumValid)
			{
				printErrInvalidData();
				return false;
			}

			*dest = varNum;
			return true;
		};

		auto checkValueFloat = [&](const std::string& val, float min, float max, float* dest) {
			float v;
			try
			{
				v = std::stof(val);
			}
			catch (std::invalid_argument e)
			{
				printErrInvalidData();
				return false;
			}

			if (v < min || v > max)
			{
				printErrInvalidData();
				return false;
			}

			*dest = v;
			return true;
		};

		std::vector<nfdu8filteritem_t> filters = { { "Flame config", "flame" } };
		std::string fileDir = FileDialog::openDialog(filters);
		if (fileDir == "")
		{
			//user closed the file dialog
			return;
		}

		std::ifstream fileStream(fileDir);
		if (!fileStream.is_open())
		{
			appendInfo("Failed to access flame config file: " + fileDir);
			return;
		}


		FlameConfig newFlameConfig{ 0 };
		std::string line;
		std::vector<std::string> lines;
		while (std::getline(fileStream, line))
		{
			lines.push_back(line);
		}

		if (lines.size() == 0)
		{
			//file is empty, interpret as wanting to have no variations
			loadFlameConfig(newFlameConfig);
			return;
		}

		cam.reset();
		for (uint32_t i = 0; i < lines.size(); i++)
		{
			line = lines[i];
			std::stringstream ssline(line);
			std::vector<std::string> values;
			std::string val;
			while (std::getline(ssline, val, ','))
			{
				values.push_back(val);
			}

			if (values.size() >= 5 && values.size() <= 10) //variation number, color L, color C, color h, weight, [translation x, y, rotation, scale x, y]
			{
				//if not all values are provided, set them to a default value
				newFlameConfig.numVariations++;
				newFlameConfig.variations[i] = 0;
				newFlameConfig.colors[i * 3] = 1.0f;
				newFlameConfig.colors[i * 3 + 1] = 1.0f;
				newFlameConfig.colors[i * 3 + 2] = 1.0f;
				newFlameConfig.weights[i] = 1.0f;
				newFlameConfig.translations[i].x = 0.0f;
				newFlameConfig.translations[i].y = 0.0f;
				newFlameConfig.rotations[i] = 0.0f;
				newFlameConfig.scales[i].x = 1.0f;
				newFlameConfig.scales[i].y = 1.0f;

				if (values.size() > 0 && !checkVarNum(values[0], &newFlameConfig.variations[i])) return;
				if (values.size() > 1 && !checkValueFloat(values[1], 0.0f, 1.0f, &newFlameConfig.colors[i * 3])) return;
				if (values.size() > 2 && !checkValueFloat(values[2], 0.0f, 1.0f, &newFlameConfig.colors[i * 3 + 1])) return;
				if (values.size() > 3 && !checkValueFloat(values[3], 0.0f, 1.0f, &newFlameConfig.colors[i * 3 + 2])) return;
				if (values.size() > 4 && !checkValueFloat(values[4], 0.0f, 1.0f, &newFlameConfig.weights[i])) return;

				//transform values don't technically have a valid range, so only checking the string value is a number
				if (values.size() > 5 && !checkValueFloat(values[5], -FLT_MAX, FLT_MAX, &newFlameConfig.translations[i].x)) return;
				if (values.size() > 6 && !checkValueFloat(values[6], -FLT_MAX, FLT_MAX, &newFlameConfig.translations[i].y)) return;
				if (values.size() > 7 && !checkValueFloat(values[7], -FLT_MAX, FLT_MAX, &newFlameConfig.rotations[i])) return;
				if (values.size() > 8 && !checkValueFloat(values[8], -FLT_MAX, FLT_MAX, &newFlameConfig.scales[i].x)) return;
				if (values.size() > 9 && !checkValueFloat(values[9], -FLT_MAX, FLT_MAX, &newFlameConfig.scales[i].y)) return;
			}
			else if (values.size() == 4) //camera x, y, angle, zoom
			{
				glm::vec2 pos;
				float angle, zoom;
				if (!checkValueFloat(values[0], -FLT_MAX, FLT_MAX, &pos.x)) return;
				if (!checkValueFloat(values[1], -FLT_MAX, FLT_MAX, &pos.y)) return;
				if (!checkValueFloat(values[2], -FLT_MAX, FLT_MAX, &angle)) return;
				if (!checkValueFloat(values[3], MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM, &zoom)) return;

				cam.updatePosition(pos);
				cam.updateRotation(angle);
				cam.updateZoom(zoom);
			}
			else if (values.size() == 3) //brightness, intensity, gamma
			{
				float _brightness, _intensity, _gamma;
				if (!checkValueFloat(values[0], MIN_BRIGHTNESS, MAX_BRIGHTNESS, &_brightness)) return;
				if (!checkValueFloat(values[1], MIN_INTENSITY, MAX_INTENSITY, &_intensity)) return;
				if (!checkValueFloat(values[2], MIN_GAMMA, MAX_GAMMA, &_gamma)) return;

				brightness = _brightness;
				intensity = _intensity;
				gamma = _gamma;
			}
			else
			{
				appendInfo("Flame config file does not match expected format, loading cancelled");
				return;
			}
		}

		loadFlameConfig(newFlameConfig);
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

		ImGui::NextColumn();

		ImGui::Text("W S A D");
		ImGui::Text("Q E");
		ImGui::Text("R F");
		ImGui::Text("Left click");
		ImGui::Text("Shift + left click");
		ImGui::Text("Ctrl + left click");

		ImGui::Columns(1);

		ImGui::TextLinkOpenURL("More Info on GitHub", "https://github.com/alexf13e/fractal-flame#usage");
		ImGui::Spacing();

		ImGui::SeparatorText("Settings");

		ImGui::PushItemWidth(UI_SAMPLE_SETTINGS_WIDTH);

		if (ImGui::Button(paused ? "Resume" : "Pause", ImVec2(UI_SAMPLE_SETTINGS_WIDTH, 0)))
		{
			paused = !paused;
		}

		if (ImGui::Button("Clear image", ImVec2(UI_SAMPLE_SETTINGS_WIDTH, 0)))
		{
			clearSingleFrame = true;
		}

		ImGui::Checkbox("Clear every frame", &clearEveryFrame);

		if (ImGui::Checkbox("Faster plotting (less accurate)", &plotWithoutAtomic))
		{
			clearSingleFrame = true;
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
				clearSingleFrame = true;
			}
		}

		float camZoom = cam.zoom;
		if (ImGui::DragFloat("Camera zoom", &camZoom, 0.025f, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM, "%.3f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_ClampOnInput))
		{
			float delta = camZoom / cam.zoom;

			if (delta != 1.0f)
			{
				cam.updateZoom(delta);
				clearSingleFrame = true;
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
				clearSingleFrame = true;
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

		if (renderTexSizeMatchPreview) ImGui::BeginDisabled();
		if (ImGui::InputInt2("Render resolution", res))
		{
			res[0] = glm::clamp(res[0], MIN_IMAGE_SIZE, MAX_IMAGE_SIZE);
			res[1] = glm::clamp(res[1], MIN_IMAGE_SIZE, MAX_IMAGE_SIZE);
			renderTexWidth = res[0];
			renderTexHeight = res[1];
		}
		if (renderTexSizeMatchPreview) ImGui::EndDisabled();

		if (ImGui::Checkbox("Match preview size", &renderTexSizeMatchPreview) && renderTexSizeMatchPreview)
		{
			renderTexWidth = previewTexWidth;
			renderTexHeight = previewTexHeight;
		}

		int n = numRenderFrames;
		if (renderFrameNumMatchPreview) ImGui::BeginDisabled();
		if (ImGui::InputInt("Number of frames", &n, 1, 10))
		{
			if (n < 0) n = 0;
			numRenderFrames = n;
		}
		if (renderFrameNumMatchPreview) ImGui::EndDisabled();

		ImGui::PopItemWidth();

		if (ImGui::Checkbox("Match preview frame num", &renderFrameNumMatchPreview) && renderFrameNumMatchPreview)
		{
			numRenderFrames = frameNum;
		}

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
		guideLineNum = 0;

		setPreviewTexSize(tw, th); //preview texture created here

		//default values for options
		renderFrameNumMatchPreview = true;
		renderTexSizeMatchPreview = true;
		if (renderTexSizeMatchPreview)
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

		setBrightness(0.5f);
		setIntensity(1.0f);
		setGamma(2.2f);

		paused = false;
		clearEveryFrame = false;
		clearSingleFrame = false;
		plotWithoutAtomic = true;
		wantsPostProcess = false;
		drawMouseLine = false;

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
			if (renderFrameNumMatchPreview) numRenderFrames = frameNum;

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
		if (renderFrameNumMatchPreview) numRenderFrames = frameNum;
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

		bool doNewRender = !(renderTexSizeMatchPreview && renderFrameNumMatchPreview);
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
		
		bool doNewRender = !(renderTexSizeMatchPreview && renderFrameNumMatchPreview);
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
