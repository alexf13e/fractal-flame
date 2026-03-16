
#include "ifs.h"

#include <stack>
#include <fstream>
#include <sstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "imgui.h"

#include "glm/gtc/constants.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"

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
		float colours[MAX_VARIATIONS * 3];
		float weights[MAX_VARIATIONS];
		float transforms[MAX_VARIATIONS * 6];

		glm::vec2 translations[MAX_VARIATIONS];
		float rotations[MAX_VARIATIONS];
		glm::vec2 scales[MAX_VARIATIONS];
	};

	namespace
	{
		ShaderProgram shFullScreenTri;
		GLuint vao_fullScreenTri;

		std::string b_previewTexture = "previewTexture";
		std::string b_previewTextureProcessed = "previewTextureProcessed";
		std::string glb_previewTextureDenoised = "previewTextureDenoised";

		std::string b_renderTexture = "renderTexture";
		std::string b_renderTextureDenoised = "renderTextureDenoised";
		std::string b_renderTextureBytes = "renderTextureBytes";

		std::string b_variations = "variations";
		std::string b_colours = "colours";
		std::string b_weights = "weights";
		std::string b_transforms = "transforms";
		std::string k_produceSamples = "produceSamples";
		std::string k_postProcess = "postProcess";
		std::string k_denoise = "denoise";
		std::string k_floatToByte = "floatToByte";
		std::vector<cl::Memory> glObjectsToAcquire;

		Camera2D cam;
		uint32_t previewTexWidth, previewTexHeight;
		uint32_t renderTexWidth, renderTexHeight;
		bool renderTexSizeMatchPreview;
		bool renderSampleNumMatchPreview;
		bool renderTransparency;

		uint32_t numPreviewSamples;
		uint32_t totalPreviewSamples;
		uint32_t numRenderSamples;
		uint32_t initialIterations;
		uint32_t iterations;
		float gamma;
		float darkness;
		uint32_t denoiseMode;

		bool clearEveryFrame;
		bool clearSingleFrame;
		bool paused;
		bool wantsPostProcess;
		bool plotWithoutAtomic;

		FlameConfig currentFlame;
		std::stack<FlameConfig> previousFlames;

		uint32_t frameNum = 0;

		constexpr uint32_t VALID_VARIATIONS[] = {
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
			16,
			18,
			19,
			20,
			27,
			28,
			29,
			31,
			34,
			35,
			42,
			43,
			48
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
			{ 16,	"Fisheye (y,x)" },
			{ 18,	"Exponential" },
			{ 19,	"Power" },
			{ 20,	"Cosine" },
			{ 27,	"Eyefish (x,y)" },
			{ 28,	"Bubble" },
			{ 29,	"Cylinder" },
			{ 31,	"Noise" },
			{ 34,	"Blur" },
			{ 35,	"Gaussian" },
			{ 42,	"Tangent" },
			{ 43,	"Square" },
			{ 48,	"Cross" }
		};

		constexpr uint32_t NUM_VALID_VARIATIONS = sizeof(VALID_VARIATIONS) / sizeof(uint32_t);

		std::unordered_map<uint32_t, const char*> DENOISE_MODES = {
			{ DENOISE_NONE, "None" },
			{ DENOISE_MEDIAN, "Median blur" },
			{ DENOISE_GAUSSIAN, "Gaussian blur" }
		};

		uint32_t NUM_DENOISE_MODES = DENOISE_MODES.size();
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
		CLManager::createBuffer<float>(b_previewTextureProcessed, numPixels * 4);
		CLManager::createGLBufferNoVAO<float>(glb_previewTextureDenoised, GL_SHADER_STORAGE_BUFFER, numPixels * 4);

		glUseProgram(shFullScreenTri.getID());
		int bufferBlockBinding = 0;
		int bufferBlockIndex = glGetProgramResourceIndex(shFullScreenTri.getID(), GL_SHADER_STORAGE_BLOCK, "TexOutput");
		glShaderStorageBlockBinding(shFullScreenTri.getID(), bufferBlockIndex, bufferBlockBinding);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferBlockBinding, CLManager::glBuffers[glb_previewTextureDenoised].glBuffer);

		glUniform1ui(glGetUniformLocation(shFullScreenTri.getID(), "texWidth"), previewTexWidth);
		glUniform1ui(glGetUniformLocation(shFullScreenTri.getID(), "texHeight"), previewTexHeight);
		glUseProgram(0);

		glObjectsToAcquire.push_back(CLManager::glBuffers[glb_previewTextureDenoised].clBuffer);
	}

	void updateCam(const glm::vec2& deltaPos, const float deltaZoom)
	{
		if (paused) return;

		cam.updatePosition(deltaPos);
		cam.updateView(deltaZoom);
		clearSingleFrame = true;
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

	bool getPaused()
	{
		return paused;
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

	void setNumPreviewSamples(uint32_t n)
	{
		//set the number of sample points which will be calculated each frame for the preview
		numPreviewSamples = n;
		clearSingleFrame = true;
	}

	void setInitialIterations(uint32_t n)
	{
		//number of iterations which will run on sample points before their positions are drawn to the buffer
		initialIterations = n;
		clearSingleFrame = true;
	}

	void setIterations(uint32_t n)
	{
		//number of iterations after top of initialIterations, where the sample position at each iteration WILL be drawn
		iterations = n;
		clearSingleFrame = true;
	}

	void setGamma(float g)
	{
		//pixel values will be raised to power of 1/gamma
		gamma = g;
		glUseProgram(shFullScreenTri.getID());
		glUniform1f(glGetUniformLocation(shFullScreenTri.getID(), "gamma"), gamma);
		glUseProgram(0);
		wantsPostProcess = true;
	}

	void setDarkness(float d)
	{
		//nicer control than setting "brightness" directly. the pixel is multiplied by brightness before gamma
		darkness = d;
		glUseProgram(shFullScreenTri.getID());
		glUniform1f(glGetUniformLocation(shFullScreenTri.getID(), "brightness"), 1.0f / darkness);
		glUseProgram(0);
		wantsPostProcess = true;
	}

	void setDenoiseMode(uint32_t m)
	{
		denoiseMode = m;
		wantsPostProcess = true;
	}

	void setRenderTransparency(bool t)
	{
		renderTransparency = t;
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
			setVariationColour(index, rgb);
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
			setVariationColour(index, rgb);
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
			currentFlame.colours[j * 3 + 0] = currentFlame.colours[(j + 1) * 3 + 0];
			currentFlame.colours[j * 3 + 1] = currentFlame.colours[(j + 1) * 3 + 1];
			currentFlame.colours[j * 3 + 2] = currentFlame.colours[(j + 1) * 3 + 2];
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
		CLManager::writeBuffer(b_variations, MAX_VARIATIONS - index - 1, currentFlame.variations, index);
		CLManager::writeBuffer(b_colours, (MAX_VARIATIONS - index - 1) * 3, currentFlame.colours, index);
		CLManager::writeBuffer(b_weights, MAX_VARIATIONS - index - 1, currentFlame.weights, index);
		CLManager::writeBuffer(b_transforms, (MAX_VARIATIONS - index - 1) * 6, currentFlame.transforms, index);

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
			std::cout << "Tried to set invalid variation: " << variation << std::endl;
			return;
		}

		currentFlame.variations[index] = variation;
		CLManager::writeBuffer(b_variations, 1, &currentFlame.variations[index], index);
		clearSingleFrame = true;
	}

	void setVariationColour(uint32_t index, glm::vec3 rgb)
	{
		if (index >= currentFlame.numVariations) return;

		currentFlame.colours[index * 3 + 0] = rgb.x;
		currentFlame.colours[index * 3 + 1] = rgb.y;
		currentFlame.colours[index * 3 + 2] = rgb.z;


		CLManager::writeBuffer(b_colours, 3, &currentFlame.colours[index * 3], index * 3);
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

		currentFlame.rotations[index] = r;
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
		CLManager::writeBuffer(b_colours, fc.numVariations * 3, currentFlame.colours);
		CLManager::writeBuffer(b_weights, fc.numVariations, currentFlame.weights);

		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			updateVariationTransform(i);
		}

		clearSingleFrame = true;
	}

	void saveFlameFile()
	{
		std::string fileName = "config";
		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			fileName += "_" + std::to_string(currentFlame.variations[i]);
		}

		std::vector<nfdu8filteritem_t> filters = {{ "Flame config", "flame" }};
		std::string fileDir = FileDialog::saveDialog(fileName, filters);
		if (fileDir == "")
		{
			//user closed the file dialog
			return;
		}

		std::ofstream fileStream(fileDir);
		if (!fileStream.is_open())
		{
			std::cout << "Failed to save flame config to file: " << fileDir << std::endl;
			return;
		}

		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			fileStream <<
				std::to_string(currentFlame.variations[i]) << "," <<
				std::to_string(currentFlame.colours[i * 3]) << "," <<
				std::to_string(currentFlame.colours[i * 3 + 1]) << "," <<
				std::to_string(currentFlame.colours[i * 3 + 2]) << "," <<
				std::to_string(currentFlame.weights[i]) << ", " <<
				std::to_string(currentFlame.translations[i].x) << ", " <<
				std::to_string(currentFlame.translations[i].y) << ", " <<
				std::to_string(currentFlame.rotations[i]) << ", " <<
				std::to_string(currentFlame.scales[i].x) << ", " <<
				std::to_string(currentFlame.scales[i].y) <<
				std::endl;
		}
	}

	void loadFlameFile()
	{
		auto printErrInvalidData = []() {
			std::cout << "Flame config file contains invalid data, loading cancelled" << std::endl;
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
			std::cout << "Failed to access flame config file: " << fileDir << std::endl;
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

		newFlameConfig.numVariations = lines.size();
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

			if (values.size() > 10) //variation number, colour L, colour C, colour h, weight, translation x, y, rotation, scale x, y
			{
				std::cout << "Flame config file does not match expected format, loading cancelled" << std::endl;
				return;
			}

			//if not all values are provided, set them to a default value
			newFlameConfig.variations[i] = 0;
			newFlameConfig.colours[i * 3] = 1.0f;
			newFlameConfig.colours[i * 3 + 1] = 1.0f;
			newFlameConfig.colours[i * 3 + 2] = 1.0f;
			newFlameConfig.weights[i] = 1.0f;
			newFlameConfig.translations[i].x = 0.0f;
			newFlameConfig.translations[i].y = 0.0f;
			newFlameConfig.rotations[i] = 0.0f;
			newFlameConfig.scales[i].x = 1.0f;
			newFlameConfig.scales[i].y = 1.0f;

			if (values.size() > 0 && !checkVarNum(values[0], &newFlameConfig.variations[i])) 							{ printErrInvalidData(); return; }
			if (values.size() > 1 && !checkValueFloat(values[1], 0.0f, 1.0f, &newFlameConfig.colours[i * 3])) 			{ printErrInvalidData(); return; }
			if (values.size() > 2 && !checkValueFloat(values[2], 0.0f, 1.0f, &newFlameConfig.colours[i * 3 + 1]))		{ printErrInvalidData(); return; }
			if (values.size() > 3 && !checkValueFloat(values[3], 0.0f, 1.0f, &newFlameConfig.colours[i * 3 + 2])) 		{ printErrInvalidData(); return; }
			if (values.size() > 4 && !checkValueFloat(values[4], 0.0f, 1.0f, &newFlameConfig.weights[i])) 				{ printErrInvalidData(); return; }
			
			//transform values don't technically have a valid range, so only checking the string value is a number
			if (values.size() > 5 && !checkValueFloat(values[5], -FLT_MAX, FLT_MAX, &newFlameConfig.translations[i].x)) { printErrInvalidData(); return; }
			if (values.size() > 6 && !checkValueFloat(values[6], -FLT_MAX, FLT_MAX, &newFlameConfig.translations[i].y)) { printErrInvalidData(); return; }
			if (values.size() > 7 && !checkValueFloat(values[7], -FLT_MAX, FLT_MAX, &newFlameConfig.rotations[i])) 		{ printErrInvalidData(); return; }
			if (values.size() > 8 && !checkValueFloat(values[8], -FLT_MAX, FLT_MAX, &newFlameConfig.scales[i].x)) 		{ printErrInvalidData(); return; }
			if (values.size() > 9 && !checkValueFloat(values[9], -FLT_MAX, FLT_MAX, &newFlameConfig.scales[i].y)) 		{ printErrInvalidData(); return; }
		}

		loadFlameConfig(newFlameConfig);
	}

	void createGUI()
	{
		constexpr float CUSTOM_UI_ITEM_WIDTH = 200.0f;
		constexpr float UI_VARIATION_SETTINGS_WIDTH = 300.0f;

		ImGui::Begin("Fractal Flame IFS", NULL, ImGuiWindowFlags_NoBackground);
		ImGui::SeparatorText("Settings");
		ImGui::PushItemWidth(CUSTOM_UI_ITEM_WIDTH);

		int temp = numPreviewSamples / 1000;
		if (ImGui::InputInt("Samples per frame (thousand)", &temp, 10, 100))
		{
			setNumPreviewSamples(std::max(temp * 1000, 0));
		}

		temp = initialIterations;
		if (ImGui::InputInt("Initial iterations", &temp))
		{
			setInitialIterations(std::max(temp, 0));
		}

		temp = iterations;
		if (ImGui::InputInt("Iterations", &temp))
		{
			setIterations(std::max(temp, 0));
		}

		float g = gamma;
		if (ImGui::DragFloat("Gamma", &g, 0.01f, 0.01f, 10.0f))
		{
			setGamma(g);
		}

		float d = darkness;
		if (ImGui::DragFloat("Darkness", &d, 0.01f, 0.1f, 10.0f))
		{
			setDarkness(d);
		}

		if (ImGui::BeginCombo("Denoising", (DENOISE_MODES[denoiseMode])))
		{
			for (uint32_t j = 0; j < NUM_DENOISE_MODES; j++)
			{
				bool is_selected = denoiseMode == j;
				if (ImGui::Selectable((DENOISE_MODES[j]), is_selected))
				{
					setDenoiseMode(j);
				}
				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Checkbox("Faster plotting (less accurate)", &plotWithoutAtomic);

		ImGui::PopItemWidth();

		ImGui::Checkbox("Clear every frame", &clearEveryFrame);

		if (ImGui::Button("Clear image"))
		{
			clearSingleFrame = true;
		}

		if (ImGui::Button("Reset camera"))
		{
			resetCam();
		}

		if (ImGui::Button(paused ? "Resume" : "Pause"))
		{
			paused = !paused;
		}

		ImGui::Spacing();

		if (ImGui::Button("Save flame config"))
		{
			saveFlameFile();
		}

		ImGui::SameLine();

		if (ImGui::Button("Load flame config"))
		{
			loadFlameFile();
		}

		if (previousFlames.size() > 0)
		{
			ImGui::SameLine();

			if (ImGui::Button("Previous flame"))
			{
				loadFlameConfig(previousFlames.top(), false);
				previousFlames.pop();
			}
		}

		ImGui::Spacing();

		ImGui::SeparatorText("Render");

		ImGui::PushItemWidth(150.0f);
		
		int res[2] = { renderTexWidth, renderTexHeight };

		if (ImGui::InputInt2("Render resolution", res, renderTexSizeMatchPreview ? ImGuiInputTextFlags_ReadOnly : 0))
		{
			if (res[0] < 1) res[0] = 1;
			if (res[1] < 1) res[1] = 1;
			renderTexWidth = res[0];
			renderTexHeight = res[1];
		}

		if (ImGui::Checkbox("Match preview size", &renderTexSizeMatchPreview) && renderTexSizeMatchPreview)
		{
			renderTexWidth = previewTexWidth;
			renderTexHeight = previewTexHeight;
		}

		int n = numRenderSamples / 1000;
		if (ImGui::InputInt("Number of samples (thousand)", &n, 1, 10, renderSampleNumMatchPreview ? ImGuiInputTextFlags_ReadOnly : 0))
		{
			if (n < 0) n = 0;
			numRenderSamples = n * 1000;
		}

		ImGui::PopItemWidth();

		if (ImGui::Checkbox("Match current preview sample num", &renderSampleNumMatchPreview) && renderSampleNumMatchPreview)
		{
			numRenderSamples = totalPreviewSamples;
		}

		if (ImGui::Checkbox("Transparent background", &renderTransparency))
		{
			setRenderTransparency(renderTransparency);
		}

		if (ImGui::Button("Save as image"))
		{
			render();
		}
		ImGui::End();


		ImGui::Begin("Variations", NULL, ImGuiWindowFlags_NoBackground);

		if (currentFlame.numVariations > 0)
		{
			ImGui::Text("Randomise");

			const float BUTTON_WIDTH = 100.0f;

			if (ImGui::Button("Variations", ImVec2(BUTTON_WIDTH, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationNum(i, VALID_VARIATIONS[randomVariationNum()]);
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Colours", ImVec2(BUTTON_WIDTH, 0.0f)))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					glm::vec3 rgb = randomOKLChtoRGB();
					setVariationColour(i, rgb);
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
					setVariationTranslation(i, glm::vec2(randomFloat(), randomFloat()));
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
					setVariationColour(i, rgb);
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

			if (ImGui::ColorEdit3("Colour", &currentFlame.colours[i * 3]))
			{
				glm::vec3 col = { currentFlame.colours[i * 3], currentFlame.colours[i * 3 + 1], currentFlame.colours[i * 3 + 2] };
				setVariationColour(i, col);
			}

			float w = currentFlame.weights[i];
			if (ImGui::SliderFloat("Weight", &w, 0.0f, 1.0f))
			{
				setVariationWeight(i, w);
			}

			float r = glm::degrees(currentFlame.rotations[i]);
			if (ImGui::DragFloat("Rotation", &r, 1.0f))
			{
				setVariationRotation(i, glm::radians(r));
			}

			float t[2] = { currentFlame.translations[i].x, currentFlame.translations[i].y };
			if (ImGui::DragFloat2("Translation", t, 0.01f))
			{
				setVariationTranslation(i, glm::vec2(t[0], t[1]));
			}


			float s[2] = { currentFlame.scales[i].x, currentFlame.scales[i].y };
			if (ImGui::DragFloat2("Scale", s, 0.01f))
			{
				setVariationScale(i, glm::vec2(s[0], s[1]));
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
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (!shFullScreenTri.init("./shaders/fullScreenTri.vert", "./shaders/ifs.frag")) return false;
		glGenVertexArrays(1, &vao_fullScreenTri);

		for (uint32_t i = 0; i < MAX_VARIATIONS; i++)
		{
			currentFlame.variations[i] = 0;
			currentFlame.colours[i * 3 + 0] = 0.0f;
			currentFlame.colours[i * 3 + 1] = 0.0f;
			currentFlame.colours[i * 3 + 2] = 0.0f;
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
		CLManager::createBuffer<float>(b_colours, MAX_VARIATIONS * 3, currentFlame.colours);
		CLManager::createBuffer<float>(b_weights, MAX_VARIATIONS, currentFlame.weights);
		CLManager::createBuffer<float>(b_transforms, MAX_VARIATIONS * 6, currentFlame.transforms);

		CLManager::createKernel(k_produceSamples);
		CLManager::createKernel(k_postProcess);
		CLManager::createKernel(k_denoise);
		CLManager::createKernel(k_floatToByte);

		cam.init(previewTexWidth, previewTexHeight, glm::vec2(0.0f));

		setPreviewTexSize(tw, th); //preview texture created here
		
		//default values for options
		renderSampleNumMatchPreview = true;
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

		setRenderTransparency(false);

		setNumPreviewSamples(50000);
		totalPreviewSamples = 0;
		numRenderSamples = 1000000;
		setInitialIterations(20);
		setIterations(5);
		setGamma(2.2f);
		setDarkness(2.0f);
		setDenoiseMode(DENOISE_NONE);

		clearEveryFrame = false;
		clearSingleFrame = false;
		paused = false;
		wantsPostProcess = false;
		plotWithoutAtomic = false;

		//start the program with 3 random variations
		addRandomVariation();
		addRandomVariation();
		addRandomVariation();

		return true;
	}

	void updateKernelParams()
	{
		CLManager::setKernelRange(k_produceSamples, numPreviewSamples);
		CLManager::setKernelParamBuffer(k_produceSamples, 0, { b_previewTexture, b_variations, b_colours, b_weights, b_transforms });
		CLManager::setKernelParamValue(k_produceSamples, 5, currentFlame.numVariations);
		CLManager::setKernelParamValue(k_produceSamples, 6, initialIterations);
		CLManager::setKernelParamValue(k_produceSamples, 7, iterations);
		CLManager::setKernelParamValue(k_produceSamples, 8, cam.getMatViewCL());
		CLManager::setKernelParamValue(k_produceSamples, 9, previewTexWidth);
		CLManager::setKernelParamValue(k_produceSamples, 10, previewTexHeight);
		CLManager::setKernelParamValue<uint8_t>(k_produceSamples, 11, plotWithoutAtomic);
		CLManager::setKernelParamValue(k_produceSamples, 12, frameNum);
		CLManager::setKernelParamValue(k_produceSamples, 13, numPreviewSamples);
		CLManager::setKernelParamLocal<uint32_t>(k_produceSamples, 14, currentFlame.numVariations);
		CLManager::setKernelParamLocal<float>(k_produceSamples, 15, currentFlame.numVariations * 3);
		CLManager::setKernelParamLocal<float>(k_produceSamples, 16, currentFlame.numVariations);
		CLManager::setKernelParamLocal<float>(k_produceSamples, 17, currentFlame.numVariations * 6);
		
		uint32_t numPixels = previewTexWidth * previewTexHeight;
		CLManager::setKernelRange(k_postProcess, numPixels);
		CLManager::setKernelParamBuffer(k_postProcess, 0, { b_previewTexture, b_previewTextureProcessed });
		CLManager::setKernelParamValue(k_postProcess, 2, gamma);
		CLManager::setKernelParamValue(k_postProcess, 3, 1.0f / darkness);
		CLManager::setKernelParamValue(k_postProcess, 4, renderTransparency);
		CLManager::setKernelParamValue(k_postProcess, 5, numPixels);

		CLManager::setKernelRange(k_denoise, numPixels);
		CLManager::setKernelParamBuffer(k_denoise, 0, { b_previewTextureProcessed });
		CLManager::setKernelParamGLBuffer(k_denoise, 1, { glb_previewTextureDenoised });
		CLManager::setKernelParamValue(k_denoise, 2, denoiseMode);
		CLManager::setKernelParamValue(k_denoise, 3, previewTexWidth);
		CLManager::setKernelParamValue(k_denoise, 4, previewTexHeight);
		CLManager::setKernelParamValue(k_denoise, 5, numPixels);
	}

	void update()
	{
		updateKernelParams();

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

		if (!paused)
		{
			if (currentFlame.numVariations > 0)
			{
				CLManager::runKernel(k_produceSamples);
				frameNum++;
				totalPreviewSamples += numPreviewSamples;
				if (renderSampleNumMatchPreview) numRenderSamples = totalPreviewSamples;
			}

			wantsPostProcess = true;
		}

		if (wantsPostProcess)
		{
			acquireGLObjects();
			CLManager::runKernel(k_postProcess);
			CLManager::runKernel(k_denoise);
			releaseGLObjects();
			wantsPostProcess = false;
		}
	}

	void clearSamples()
	{
		//clear the preview buffer and start from 0 samples
		CLManager::fillBuffer(b_previewTexture, previewTexWidth * previewTexHeight * 4, 0);
		totalPreviewSamples = 0;
		frameNum = 0;
		if (renderSampleNumMatchPreview) numRenderSamples = totalPreviewSamples;
		wantsPostProcess = true;
	}

	void draw()
	{
		//draw the preview buffer to the screen
		glUseProgram(shFullScreenTri.getID());
		glBindVertexArray(vao_fullScreenTri);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);
		glUseProgram(0);
	}

	void render()
	{
		//render to an image file

		//set save path
		std::string fileName = std::to_string(numRenderSamples);
		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			fileName += "_" + std::to_string(currentFlame.variations[i]);
		}

		std::vector<nfdu8filteritem_t> filters = { { "PNG Image", "png" } };
		std::string renderOutputPath = FileDialog::saveDialog(fileName, filters);
		if (renderOutputPath == "")
		{
			//pressed cancel, so don't render
			return;
		}

		std::cout << "Rendering..." << std::endl;

		uint32_t numPixels = renderTexWidth * renderTexHeight;
		CLManager::createBuffer<float>(b_renderTexture, numPixels * 4);
		CLManager::createBuffer<uint8_t>(b_renderTextureBytes, numPixels * 4);

		//produce the samples on the texture
		CLManager::setKernelRange(k_produceSamples, numRenderSamples);
		CLManager::setKernelParamBuffer(k_produceSamples, 0, { b_renderTexture });
		cam.setAspectRatio(renderTexWidth, renderTexHeight);
		CLManager::setKernelParamValue(k_produceSamples, 8, cam.getMatViewCL());
		CLManager::setKernelParamValue(k_produceSamples, 9, renderTexWidth);
		CLManager::setKernelParamValue(k_produceSamples, 10, renderTexHeight);
		CLManager::setKernelParamValue(k_produceSamples, 11, plotWithoutAtomic);
		CLManager::setKernelParamValue(k_produceSamples, 12, 0);
		CLManager::setKernelParamValue(k_produceSamples, 13, numRenderSamples);
		CLManager::runKernel(k_produceSamples);
		
		std::cout << "Applying post process..." << std::endl;

		//apply brightness and gamma and convert from float to byte
		CLManager::setKernelRange(k_postProcess, numPixels);
		CLManager::setKernelParamBuffer(k_postProcess, 0, { b_renderTexture, b_renderTexture });
		CLManager::setKernelParamValue(k_postProcess, 5, numPixels);
		CLManager::runKernel(k_postProcess);

		CLManager::createBuffer<float>(b_renderTextureDenoised, numPixels * 4);
		CLManager::setKernelRange(k_denoise, numPixels);
		CLManager::setKernelParamBuffer(k_denoise, 0, { b_renderTexture, b_renderTextureDenoised });
		CLManager::setKernelParamValue(k_denoise, 3, renderTexWidth);
		CLManager::setKernelParamValue(k_denoise, 4, renderTexHeight);
		CLManager::setKernelParamValue(k_denoise, 5, numPixels);
		CLManager::runKernel(k_denoise);

		CLManager::setKernelRange(k_floatToByte, numPixels);
		CLManager::setKernelParamBuffer(k_floatToByte, 0, { b_renderTextureDenoised, b_renderTextureBytes });
		CLManager::setKernelParamValue(k_floatToByte, 2, numPixels);
		CLManager::runKernel(k_floatToByte);

		std::cout << "Saving to " << renderOutputPath << std::endl;

		//save the texture to an image
		uint8_t* texture = new uint8_t[numPixels * 4];
		CLManager::readBuffer(b_renderTextureBytes, numPixels * 4, texture);
		stbi_write_png_compression_level = 1;
		stbi_flip_vertically_on_write(1);
		stbi_write_png(renderOutputPath.c_str(), renderTexWidth, renderTexHeight, 4, texture, renderTexWidth * 4 * sizeof(uint8_t));
		delete[] texture;

		std::cout << "Render complete" << std::endl;

		cam.setAspectRatio(previewTexWidth, previewTexHeight);
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

		//generate a random "sensible" colour in LCh space
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
}
