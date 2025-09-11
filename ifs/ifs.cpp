
#include "ifs.h"

#include <random>

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
	namespace //put variables inside private namespace to prevent modifying them without using setter functions
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

		uint32_t numVariations;
		uint32_t variations[MAX_VARIATIONS];
		float coloursRGB[MAX_VARIATIONS * 3];
		float coloursLCh[MAX_VARIATIONS * 3];
		float weights[MAX_VARIATIONS];

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
			18,
			19,
			20,
			28,
			29,
			42,
			48
		};
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
		//get rid of previous gl buffer in list of objects to acquire
		/*for (uint32_t i = 0; i < glObjectsToAcquire.size(); i++)
		{
			if (glObjectsToAcquire[i]() == CLManager::glBuffers[glb_previewTexture].clBuffer())
			{
				glObjectsToAcquire.erase(glObjectsToAcquire.begin() + i);
				break;
			}
		}*/

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
		if (numVariations < MAX_VARIATIONS)
		{
			uint32_t index = numVariations;
			numVariations++;

			setVariationNum(index, 0);
			setVariationColour(index, 1.0f, 0.0f, 0.0f);
			setVariationWeight(index, 1.0f);

			CLManager::setKernelParamValue(k_produceSamples, 4, numVariations);
			clearSingleFrame = true;
		}
	}

	void addRandomVariation()
	{
		//shortcut for adding variation with randomised parameters
		if (numVariations < MAX_VARIATIONS)
		{
			uint32_t index = numVariations;
			numVariations++;

			setVariationNum(index, VALID_VARIATIONS[randomVariationIndex()]);
			float* col = randomOKLCh();
			setVariationColour(index, col[0], col[1], col[2]);
			setVariationWeight(index, randomFloat());

			CLManager::setKernelParamValue(k_produceSamples, 4, numVariations);
			clearSingleFrame = true;
		}
	}

	void removeVariation(uint32_t index)
	{
		//shift variations down after the deleted one
		for (uint32_t j = index; j < numVariations - 1; j++)
		{
			variations[j] = variations[j + 1];
			coloursRGB[j * 3 + 0] = coloursRGB[(j + 1) * 3 + 0];
			coloursRGB[j * 3 + 1] = coloursRGB[(j + 1) * 3 + 1];
			coloursRGB[j * 3 + 2] = coloursRGB[(j + 1) * 3 + 2];
			weights[j] = weights[j + 1];
		}

		numVariations--;

		//reset values in unused variation (not really necessary)
		variations[numVariations] = 0;
		coloursRGB[numVariations * 3 + 0] = 0.0f;
		coloursRGB[numVariations * 3 + 1] = 0.0f;
		coloursRGB[numVariations * 3 + 2] = 0.0f;
		weights[numVariations] = 0.0f;

		//update kernel buffer parameters
		CLManager::writeBuffer(b_variations, MAX_VARIATIONS, variations);
		CLManager::writeBuffer(b_colours, MAX_VARIATIONS * 3, coloursRGB);
		CLManager::writeBuffer(b_weights, MAX_VARIATIONS, weights);
		CLManager::setKernelParamValue(k_produceSamples, 4, numVariations);

		clearSingleFrame = true;
	}

	void setVariationNum(uint32_t index, uint32_t variation)
	{
		if (index >= numVariations) return;

		bool valid = false;
		for (uint32_t i = 0; i < IM_ARRAYSIZE(VALID_VARIATIONS); i++)
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

		variations[index] = variation;
		CLManager::writeBuffer(b_variations, 1, &variations[index], index);
		clearSingleFrame = true;
	}

	void setVariationColour(uint32_t index, float L, float C, float h)
	{
		if (index >= numVariations) return;

		coloursLCh[index * 3 + 0] = L;
		coloursLCh[index * 3 + 1] = C;
		coloursLCh[index * 3 + 2] = h;

		float* rgb = OKLChtoRGB(&coloursLCh[index * 3]);
		coloursRGB[index * 3 + 0] = rgb[0];
		coloursRGB[index * 3 + 1] = rgb[1];
		coloursRGB[index * 3 + 2] = rgb[2];

		CLManager::writeBuffer(b_colours, 3, &coloursRGB[index * 3], index * 3);
		clearSingleFrame = true;
	}

	void setVariationWeight(uint32_t index, float w)
	{
		if (index >= numVariations) return;
		
		weights[index] = w;
		CLManager::writeBuffer(b_weights, 1, &weights[index], index);
		clearSingleFrame = true;
	}

	void createGUI()
	{
		#define IMGUI_SPACER ImGui::Dummy(ImVec2(0.0f, 10.0f));

		ImGui::Begin("IFS", NULL);
		ImGui::SeparatorText("Settings");

		int temp = numPreviewSamples;
		if (ImGui::InputInt("Samples per frame", &temp, 10000, 100000))
		{
			setNumPreviewSamples(std::max(temp, 0));
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
		if (ImGui::DragFloat("Gamma", &g, 0.01f, 0.00001f, 10.0f))
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

		ImGui::SeparatorText("Variations");

		for (uint32_t i = 0; i < numVariations; i++)
		{
			ImGui::PushID(i);

			if (ImGui::BeginCombo("Variation", std::to_string(variations[i]).c_str()))
			{
				for (uint32_t j = 0; j < IM_ARRAYSIZE(VALID_VARIATIONS); j++)
				{
					bool is_selected = variations[i] == VALID_VARIATIONS[j];
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

			//can't use sliderfloat3 as each value has a different range
			ImGui::PushItemWidth(90.0f);
			if (ImGui::SliderFloat("##L", &coloursLCh[i * 3], 0.0f, 1.0f))
			{
				coloursLCh[i * 3] = glm::clamp(coloursLCh[i * 3], 0.0f, 1.0f);
				setVariationColour(i, coloursLCh[i * 3], coloursLCh[i * 3 + 1], coloursLCh[i * 3 + 2]);
			}
			ImGui::SameLine();
			if (ImGui::SliderFloat("##C", &coloursLCh[i * 3 + 1], 0.0f, 0.5f))
			{
				coloursLCh[i * 3 + 1] = glm::clamp(coloursLCh[i * 3 + 1], 0.0f, 1.0f);
				setVariationColour(i, coloursLCh[i * 3], coloursLCh[i * 3 + 1], coloursLCh[i * 3 + 2]);
			}
			ImGui::SameLine();
			if (ImGui::SliderFloat("L C h", &coloursLCh[i * 3 + 2], 0.0f, 2.0f * PI))
			{
				coloursLCh[i * 3 + 2] = glm::clamp(coloursLCh[i * 3 + 2], 0.0f, 2.0f * PI);
				setVariationColour(i, coloursLCh[i * 3], coloursLCh[i * 3 + 1], coloursLCh[i * 3 + 2]);
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImVec4 col = ImVec4(coloursRGB[i * 3], coloursRGB[i * 3 + 1], coloursRGB[i * 3 + 2], 1.0f);
			ImGui::ColorButton("Colour", col);

			float w = weights[i];
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

		if (numVariations < MAX_VARIATIONS)
		{
			if (ImGui::Button("Add variation"))
			{
				addDefaultVariation();
			}
		}

		if (ImGui::Button("Randomise variations"))
		{
			for (uint32_t i = 0; i < numVariations; i++)
			{
				setVariationNum(i, VALID_VARIATIONS[randomVariationIndex()]);
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Randomise colours"))
		{
			for (uint32_t i = 0; i < numVariations; i++)
			{
				float* colour = randomOKLCh();
				setVariationColour(i, colour[0], colour[1], colour[2]);
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Randomise weights"))
		{
			for (uint32_t i = 0; i < numVariations; i++)
			{
				setVariationWeight(i, randomFloat());
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

		int n = numRenderSamples;
		if (ImGui::InputInt("Number of samples", &n, 1000000, 10000000, renderMatchPreviewSampleNum ? ImGuiInputTextFlags_ReadOnly : 0))
		{
			if (n < 0) n = 0;
			numRenderSamples = n;
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
			variations[i] = 0;
			coloursRGB[i * 3 + 0] = 0.0f;
			coloursRGB[i * 3 + 1] = 0.0f;
			coloursRGB[i * 3 + 2] = 0.0f;
			weights[i] = 0.0f;
		}

		CLManager::createBuffer<uint32_t>(b_variations, MAX_VARIATIONS, variations);
		CLManager::createBuffer<float>(b_colours, MAX_VARIATIONS * 3, coloursRGB);
		CLManager::createBuffer<float>(b_weights, MAX_VARIATIONS, weights);

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

		numRenderSamples = 1e6;
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

		if (!paused && numVariations > 0)
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
		std::string fileName = "flame";
		for (uint32_t i = 0; i < numVariations; i++)
		{
			fileName += "_" + std::to_string(variations[i]);
		}

		std::string renderOutputPath = FileDialog::saveDialog(fileName);
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

	uint32_t randomVariationIndex()
	{
		//don't want variation 0
		return (uint32_t)(1 + randomFloat() * (MAX_VARIATIONS - 1));
	}

	float* randomOKLCh()
	{
		//generate a random "sensible" colour in LCh space
		float L = 0.3f + randomFloat() * 0.5f;
		float C = randomFloat() * 0.5f;
		float h = randomFloat() * 2.0f * PI;

		float okLCh[3] = { L, C, h };
		return okLCh;
	}

	float* OKLChtoRGB(float okLCh[3])
	{
		//https://bottosson.github.io/posts/oklab/

		float a = okLCh[1] * cos(okLCh[2]);
		float b = okLCh[1] * sin(okLCh[2]);
		float okLAB[3] = { okLCh[0], a, b };

		float l_ = okLAB[0] + 0.3963377774f * okLAB[1] + 0.2158037573f * okLAB[2];
		float m_ = okLAB[0] - 0.1055613458f * okLAB[1] - 0.0638541728f * okLAB[2];
		float s_ = okLAB[0] - 0.0894841775f * okLAB[1] - 1.2914855480f * okLAB[2];

		float l = l_ * l_ * l_;
		float m = m_ * m_ * m_;
		float s = s_ * s_ * s_;

		float rgb[3] = {
			+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
			-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
			-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
		};

		rgb[0] = glm::clamp(rgb[0], 0.0f, 1.0f);
		rgb[1] = glm::clamp(rgb[1], 0.0f, 1.0f);
		rgb[2] = glm::clamp(rgb[2], 0.0f, 1.0f);

		return rgb;
	}
}