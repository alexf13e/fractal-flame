
#include "ifs.h"

#include <random>
#include <stack>
#include <fstream>
#include <sstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "imgui.h"

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
	};

	namespace
	{
		ShaderProgram shFullScreenTri;
		GLuint vao_fullScreenTri;

		std::string glb_previewTexture = "previewTexture";
		std::string b_renderTexture = "renderTexture";
		std::string b_processedRenderTexture = "processedRenderTexture";
		std::string b_variations = "variations";
		std::string b_colours = "colours";
		std::string b_weights = "weights";
		std::string k_produceSamples = "produceSamples";
		std::string k_renderPostProcess = "renderPostProcess";
		std::vector<cl::Memory> glObjectsToAcquire;

		Camera2D cam;
		uint32_t previewTexWidth, previewTexHeight;
		uint32_t renderTexWidth, renderTexHeight;
		bool renderTransparency;

		uint32_t numPreviewSamples;
		uint32_t totalPreviewSamples;
		uint32_t numRenderSamples;
		uint32_t initialIterations;
		uint32_t iterations;

		bool clearEveryFrame;
		bool clearSingleFrame;
		bool paused;
		bool renderMatchPreviewSampleNum;

		float gamma;
		float darkness;

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
		constexpr uint32_t NUM_VALID_VARIATIONS = sizeof(VALID_VARIATIONS) / sizeof(uint32_t);
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
		CLManager::createGLBufferNoVAO<float>(glb_previewTexture, GL_SHADER_STORAGE_BUFFER, numPixels * 4);

		glUseProgram(shFullScreenTri.getID());
		//use shader storage buffer as easier to work with between opencl and gl
		int bufferBlockBinding = 0;
		int bufferBlockIndex = glGetProgramResourceIndex(shFullScreenTri.getID(), GL_SHADER_STORAGE_BLOCK, "TexOutput");
		glShaderStorageBlockBinding(shFullScreenTri.getID(), bufferBlockIndex, bufferBlockBinding);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bufferBlockBinding, CLManager::glBuffers[glb_previewTexture].glBuffer);

		glUniform1ui(glGetUniformLocation(shFullScreenTri.getID(), "texWidth"), previewTexWidth);
		glUniform1ui(glGetUniformLocation(shFullScreenTri.getID(), "texHeight"), previewTexHeight);
		glUseProgram(0);

		glObjectsToAcquire.push_back(CLManager::glBuffers[glb_previewTexture].clBuffer);

		//update relevent kernel parameters for resized buffer
		CLManager::setKernelParamGLBuffer(k_produceSamples, 0, { glb_previewTexture });
		CLManager::setKernelParamValue(k_produceSamples, 8, previewTexWidth);
		CLManager::setKernelParamValue(k_produceSamples, 9, previewTexHeight);
	}

	void updateCam(const glm::vec2& deltaPos, const float deltaZoom)
	{
		if (paused) return;

		cam.updatePosition(deltaPos);
		cam.updateView(deltaZoom);
		CLManager::setKernelParamValue(k_produceSamples, 7, cam.getMatViewCL());
		clearSingleFrame = true;
	}

	void resetCam()
	{
		cam.reset();
		CLManager::setKernelParamValue(k_produceSamples, 7, cam.getMatViewCL());
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
		createPreviewTexture();
		cam.setAspectRatio(previewTexWidth, previewTexHeight);
		CLManager::setKernelParamValue(k_produceSamples, 7, cam.getMatViewCL());
	}

	void setNumPreviewSamples(uint32_t n)
	{
		//set the number of sample points which will be calculated each frame for the preview
		numPreviewSamples = n;
		CLManager::setKernelRange(k_produceSamples, numPreviewSamples);
		CLManager::setKernelParamValue(k_produceSamples, 11, numPreviewSamples);
		clearSingleFrame = true;
	}

	void setInitialIterations(uint32_t n)
	{
		//number of iterations which will run on sample points before their positions are drawn to the buffer
		initialIterations = n;
		CLManager::setKernelParamValue(k_produceSamples, 5, initialIterations);
		clearSingleFrame = true;
	}

	void setIterations(uint32_t n)
	{
		//number of iterations after top of initialIterations, where the sample position at each iteration WILL be drawn
		iterations = n;
		CLManager::setKernelParamValue(k_produceSamples, 6, iterations);
		clearSingleFrame = true;
	}

	void setGamma(float g)
	{
		//pixel values will be raised to power of 1/gamma
		gamma = g;
		glUseProgram(shFullScreenTri.getID());
		glUniform1f(glGetUniformLocation(shFullScreenTri.getID(), "gamma"), gamma);
		glUseProgram(0);
		CLManager::setKernelParamValue(k_renderPostProcess, 2, gamma);
	}

	void setDarkness(float b)
	{
		//nicer control than setting "brightness" directly. the pixel is multiplied by brightness before gamma
		darkness = b;
		glUseProgram(shFullScreenTri.getID());
		glUniform1f(glGetUniformLocation(shFullScreenTri.getID(), "brightness"), 1.0f / darkness);
		glUseProgram(0);
		CLManager::setKernelParamValue(k_renderPostProcess, 3, 1.0f / darkness);
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

			CLManager::setKernelParamValue(k_produceSamples, 4, currentFlame.numVariations);
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

			CLManager::setKernelParamValue(k_produceSamples, 4, currentFlame.numVariations);
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
		}

		currentFlame.numVariations--;

		//update kernel buffer parameters
		CLManager::writeBuffer(b_variations, MAX_VARIATIONS, currentFlame.variations);
		CLManager::writeBuffer(b_colours, MAX_VARIATIONS * 3, currentFlame.colours);
		CLManager::writeBuffer(b_weights, MAX_VARIATIONS, currentFlame.weights);
		CLManager::setKernelParamValue(k_produceSamples, 4, currentFlame.numVariations);

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

	void loadFlameConfig(FlameConfig fc, bool savePrevFlame)
	{
		if (savePrevFlame) previousFlames.push(currentFlame);

		currentFlame = fc;

		CLManager::writeBuffer(b_variations, MAX_VARIATIONS, currentFlame.variations);
		CLManager::writeBuffer(b_colours, MAX_VARIATIONS * 3, currentFlame.colours);
		CLManager::writeBuffer(b_weights, MAX_VARIATIONS, currentFlame.weights);
		CLManager::setKernelParamValue(k_produceSamples, 4, currentFlame.numVariations);

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
				std::to_string(currentFlame.weights[i]) <<
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

			if (values.size() != 5) //variation number, colour L, colour C, colour h, weight
			{
				std::cout << "Flame config file does not match expected format, loading cancelled" << std::endl;
				return;
			}

			if (!checkVarNum(values[0], &newFlameConfig.variations[i])) return;
			if (!checkValueFloat(values[1], 0.0f, 1.0f, &newFlameConfig.colours[i * 3])) return;
			if (!checkValueFloat(values[2], 0.0f, 1.0f, &newFlameConfig.colours[i * 3 + 1])) return;
			if (!checkValueFloat(values[3], 0.0f, 1.0f, &newFlameConfig.colours[i * 3 + 2])) return;
			if (!checkValueFloat(values[4], 0.0f, 1.0f, &newFlameConfig.weights[i])) return;
		}

		loadFlameConfig(newFlameConfig);
	}

	void createGUI()
	{
		#define IMGUI_SPACER ImGui::Dummy(ImVec2(0.0f, 10.0f));

		ImGui::Begin("IFS", NULL);
		ImGui::SeparatorText("Settings");

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

		IMGUI_SPACER

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

		IMGUI_SPACER

		ImGui::SeparatorText("Variations");

		if (currentFlame.numVariations > 0)
		{
			if (ImGui::Button("Randomise variations"))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationNum(i, VALID_VARIATIONS[randomVariationNum()]);
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Randomise colours"))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					glm::vec3 rgb = randomOKLChtoRGB();
					setVariationColour(i, rgb);
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Randomise weights"))
			{
				previousFlames.push(currentFlame);

				for (uint32_t i = 0; i < currentFlame.numVariations; i++)
				{
					setVariationWeight(i, randomFloat());
				}
			}

			ImGui::Separator();
		}


		for (uint32_t i = 0; i < currentFlame.numVariations; i++)
		{
			ImGui::PushID(i);

			if (ImGui::BeginCombo("Variation", std::to_string(currentFlame.variations[i]).c_str()))
			{
				for (uint32_t j = 0; j < NUM_VALID_VARIATIONS; j++)
				{
					bool is_selected = currentFlame.variations[i] == VALID_VARIATIONS[j];
					if (ImGui::Selectable(std::to_string(VALID_VARIATIONS[j]).c_str(), is_selected))
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

			if (ImGui::Button("Remove"))
			{
				removeVariation(i);
			}

			IMGUI_SPACER

			ImGui::Separator();

			ImGui::PopID();
		}

		if (currentFlame.numVariations < MAX_VARIATIONS)
		{
			if (ImGui::Button("Add variation"))
			{
				addDefaultVariation();
			}
		}

		ImGui::End();

		ImGui::Begin("Render");

		int res[2] = { renderTexWidth, renderTexHeight };
		if (ImGui::InputInt2("Render resolution", res))
		{
			if (res[0] < 1) res[0] = 1;
			if (res[1] < 1) res[1] = 1;
			renderTexWidth = res[0];
			renderTexHeight = res[1];
		}

		int n = numRenderSamples / 1000;
		if (ImGui::InputInt("Number of samples (thousand)", &n, 1, 10, renderMatchPreviewSampleNum ? ImGuiInputTextFlags_ReadOnly : 0))
		{
			if (n < 0) n = 0;
			numRenderSamples = n * 1000;
		}

		if (ImGui::Checkbox("Match current preview sample num", &renderMatchPreviewSampleNum) && renderMatchPreviewSampleNum)
		{
			numRenderSamples = totalPreviewSamples;
		}

		ImGui::Checkbox("Transparent background", &renderTransparency);

		if (ImGui::Button("Render"))
		{
			render();
		}

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
		}

		CLManager::createBuffer<uint32_t>(b_variations, MAX_VARIATIONS, currentFlame.variations);
		CLManager::createBuffer<float>(b_colours, MAX_VARIATIONS * 3, currentFlame.colours);
		CLManager::createBuffer<float>(b_weights, MAX_VARIATIONS, currentFlame.weights);

		CLManager::createKernel(k_produceSamples);
		CLManager::createKernel(k_renderPostProcess);

		CLManager::setKernelParamBuffer(k_produceSamples, 1, { b_variations, b_colours, b_weights });
		CLManager::setKernelParamLocal(k_produceSamples, 12, MAX_VARIATIONS * sizeof(uint32_t));
		CLManager::setKernelParamLocal(k_produceSamples, 13, MAX_VARIATIONS * 3 * sizeof(float));
		CLManager::setKernelParamLocal(k_produceSamples, 14, MAX_VARIATIONS * sizeof(float));

		cam.init(previewTexWidth, previewTexHeight, glm::vec2(0.0f));

		setPreviewTexSize(tw, th); //preview texture created here
		setNumPreviewSamples(10000);
		setInitialIterations(20);
		setIterations(5);
		setGamma(2.2f);
		setDarkness(2.0f);

		numRenderSamples = 1000000;
		totalPreviewSamples = 0;
		renderTexWidth = 1920;
		renderTexHeight = 1080;
		renderTransparency = false;
		renderMatchPreviewSampleNum = true;

		clearEveryFrame = false;
		clearSingleFrame = false;
		paused = false;

		addRandomVariation();
		addRandomVariation();
		addRandomVariation();

		return true;
	}

	void update()
	{
		if (!paused && clearEveryFrame)
		{
			clearSamples();
		}

		if (clearSingleFrame)
		{
			clearSamples();
			clearSingleFrame = false;
		}

		if (!paused && currentFlame.numVariations > 0)
		{
			acquireGLObjects();

			CLManager::setKernelParamValue(k_produceSamples, 10, frameNum);
			CLManager::runKernel(k_produceSamples);
			
			releaseGLObjects();
		}

		if (!paused)
		{
			frameNum++;
			totalPreviewSamples += numPreviewSamples;
			if (renderMatchPreviewSampleNum) numRenderSamples = totalPreviewSamples;
		}
	}

	void clearSamples()
	{
		//clear the preview buffer and start from 0 samples
		glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_RGBA32F, GL_RGBA, GL_FLOAT, NULL);
		totalPreviewSamples = 0;
		if (renderMatchPreviewSampleNum) numRenderSamples = totalPreviewSamples;
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
		CLManager::createBuffer<uint8_t>(b_processedRenderTexture, numPixels * 4);
		

		//produce the samples on the texture
		CLManager::setKernelRange(k_produceSamples, numRenderSamples);
		CLManager::setKernelParamBuffer(k_produceSamples, 0, { b_renderTexture });
		cam.setAspectRatio(renderTexWidth, renderTexHeight);
		CLManager::setKernelParamValue(k_produceSamples, 7, cam.getMatViewCL());
		CLManager::setKernelParamValue(k_produceSamples, 8, renderTexWidth);
		CLManager::setKernelParamValue(k_produceSamples, 9, renderTexHeight);
		CLManager::setKernelParamValue(k_produceSamples, 11, numRenderSamples);
		CLManager::runKernel(k_produceSamples);

		std::cout << "Applying post process..." << std::endl;

		//apply brightness and gamma and convert from float to byte
		CLManager::setKernelRange(k_renderPostProcess, numPixels);
		CLManager::setKernelParamBuffer(k_renderPostProcess, 0, { b_renderTexture, b_processedRenderTexture });
		CLManager::setKernelParamValue(k_renderPostProcess, 2, gamma);
		CLManager::setKernelParamValue(k_renderPostProcess, 3, 1.0f / darkness);
		CLManager::setKernelParamValue(k_renderPostProcess, 4, renderTransparency);
		CLManager::setKernelParamValue(k_renderPostProcess, 5, numPixels);
		CLManager::runKernel(k_renderPostProcess);

		std::cout << "Saving to " << renderOutputPath << std::endl;

		//save the texture to an image
		uint8_t* texture = new uint8_t[numPixels * 4];
		CLManager::readBuffer(b_processedRenderTexture, numPixels * 4, texture);
		stbi_write_png_compression_level = 1;
		stbi_flip_vertically_on_write(1);
		stbi_write_png(renderOutputPath.c_str(), renderTexWidth, renderTexHeight, 4, texture, renderTexWidth * 4 * sizeof(uint8_t));
		delete[] texture;

		std::cout << "Render complete" << std::endl;

		//put preview kernel parameters back
		CLManager::setKernelRange(k_produceSamples, numPreviewSamples);
		CLManager::setKernelParamGLBuffer(k_produceSamples, 0, { glb_previewTexture });
		cam.setAspectRatio(previewTexWidth, previewTexHeight);
		CLManager::setKernelParamValue(k_produceSamples, 7, cam.getMatViewCL());
		CLManager::setKernelParamValue(k_produceSamples, 8, previewTexWidth);
		CLManager::setKernelParamValue(k_produceSamples, 9, previewTexHeight);
		CLManager::setKernelParamValue(k_produceSamples, 11, numPreviewSamples);
	}

	void destroy()
	{

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