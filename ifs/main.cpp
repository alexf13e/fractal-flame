
#include <iostream>
#include <map>

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define CL_MANAGER_GL
#include "CLManager.h"
#include "Key.h"
#include "kernels.h"
#include "filedialog.h"

#include "ifs.h"


GLFWwindow* window;
std::chrono::steady_clock::time_point prevFrameEndTime;
float frameDuration;
bool firstFrame;

float moveMult = 1.0f;

static std::map<int, Key> keyMap = {
	{GLFW_KEY_A, Key()},
	{GLFW_KEY_B, Key()},
	{GLFW_KEY_C, Key()},
	{GLFW_KEY_D, Key()},
	{GLFW_KEY_E, Key()},
	{GLFW_KEY_F, Key()},
	{GLFW_KEY_G, Key()},
	{GLFW_KEY_H, Key()},
	{GLFW_KEY_I, Key()},
	{GLFW_KEY_J, Key()},
	{GLFW_KEY_K, Key()},
	{GLFW_KEY_L, Key()},
	{GLFW_KEY_M, Key()},
	{GLFW_KEY_N, Key()},
	{GLFW_KEY_O, Key()},
	{GLFW_KEY_P, Key()},
	{GLFW_KEY_Q, Key()},
	{GLFW_KEY_R, Key()},
	{GLFW_KEY_S, Key()},
	{GLFW_KEY_T, Key()},
	{GLFW_KEY_U, Key()},
	{GLFW_KEY_V, Key()},
	{GLFW_KEY_W, Key()},
	{GLFW_KEY_X, Key()},
	{GLFW_KEY_Y, Key()},
	{GLFW_KEY_Z, Key()},
	{GLFW_KEY_0, Key()},
	{GLFW_KEY_1, Key()},
	{GLFW_KEY_2, Key()},
	{GLFW_KEY_3, Key()},
	{GLFW_KEY_4, Key()},
	{GLFW_KEY_5, Key()},
	{GLFW_KEY_6, Key()},
	{GLFW_KEY_7, Key()},
	{GLFW_KEY_8, Key()},
	{GLFW_KEY_9, Key()},
	{GLFW_KEY_LEFT, Key()},
	{GLFW_KEY_RIGHT, Key()},
	{GLFW_KEY_UP, Key()},
	{GLFW_KEY_DOWN, Key()},
	{GLFW_KEY_LEFT_SHIFT, Key()},
	{GLFW_KEY_LEFT_CONTROL, Key()},
	{GLFW_MOUSE_BUTTON_LEFT, Key()},
};

glm::vec2 prevMousePos, currentMousePos;
bool firstMouseMovement;

bool showGUI;


void onKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (keyMap.count(key) == 0)
	{
		//key pressed which is not set to be used in the keymap
		return;
	}

	switch (action)
	{
	case GLFW_PRESS:
		keyMap.at(key).setDown();
		break;

	case GLFW_RELEASE:
		keyMap.at(key).setUp();
		break;
	}
}

void onMouseClick(GLFWwindow* window, int button, int action, int mods)
{
	if (ImGui::GetIO().WantCaptureMouse) return;

	if (keyMap.count(button) == 0) return;

	if (action == GLFW_PRESS)
	{
		keyMap.at(button).setDown();
	}
	else if (action == GLFW_RELEASE)
	{
		keyMap.at(button).setUp();
	}
}

void onWindowResize(GLFWwindow* w, int width, int height)
{
	if (width == 0 || height == 0) return;

	glViewport(0, 0, width, height);
	ifs::setPreviewTexSize(width, height);
	ifs::clearSamples();
}

bool init()
{
	srand(std::chrono::steady_clock::now().time_since_epoch().count());

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(1280, 720, "Fractal Flame IFS", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}

	glfwMaximizeWindow(window);

	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwTerminate();
		return false;
	}

	glfwSetFramebufferSizeCallback(window, onWindowResize);
	glfwSetKeyCallback(window, onKeyPress);
	glfwSetMouseButtonCallback(window, onMouseClick);
	glfwSwapInterval(0);

	if (!CLManager::init(window, createKernelSource()))
	{
		std::cout << "failed to initialise CLManager, exiting" << std::endl;
		glfwTerminate();
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
	
	float xscale, yscale;
	glfwGetWindowContentScale(window, &xscale, &yscale);
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(1.5f);
	style.FontScaleDpi = xscale;


	int initialWindowWidth, initialWindowHeight;
	glfwGetWindowSize(window, &initialWindowWidth, &initialWindowHeight);
	if (!ifs::init(initialWindowWidth, initialWindowHeight)) return false;

	FileDialog::init(window);

	//for some reason on Windows, the window doesn't detect being maximised and the preview texture size is wrong...
	onWindowResize(window, initialWindowWidth, initialWindowHeight);

	firstFrame = true;
	currentMousePos = glm::vec2(-1.0f);
	prevMousePos = glm::vec2(-1.0f);
	firstMouseMovement = true;

	showGUI = true;

	return true;
}

bool update()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	//Camera controls
	glm::vec4 moveInputs = glm::vec4(0.0f);
	if (keyMap.at(GLFW_KEY_W).getHeld()) moveInputs.y += 1.0f;
	if (keyMap.at(GLFW_KEY_S).getHeld()) moveInputs.y -= 1.0f;
	if (keyMap.at(GLFW_KEY_A).getHeld()) moveInputs.x -= 1.0f;
	if (keyMap.at(GLFW_KEY_D).getHeld()) moveInputs.x += 1.0f;
	if (keyMap.at(GLFW_KEY_F).getHeld()) moveInputs.z -= 1.0f;
	if (keyMap.at(GLFW_KEY_R).getHeld()) moveInputs.z += 1.0f;
	if (keyMap.at(GLFW_KEY_Q).getHeld()) moveInputs.w += 1.0f;
	if (keyMap.at(GLFW_KEY_E).getHeld()) moveInputs.w -= 1.0f;

	if (keyMap.at(GLFW_KEY_C).getReleased()) moveMult *= 2.0f;
	if (keyMap.at(GLFW_KEY_X).getReleased()) moveMult *= 0.5f;

	if (keyMap.at(GLFW_KEY_H).getReleased()) showGUI = !showGUI;

	//print maybe useful debug info
	if (keyMap.at(GLFW_KEY_P).getReleased())
	{
		std::cout << "frame duration: " << frameDuration << std::endl;
		std::cout << "memory usage: " << CLManager::getTotalBufferMemUsageMB() << "MB" << std::endl << std::endl;
	}

	//calculate camera movement
	const float minMoveMult = 1.0f / (1 << 10);
	if (moveMult < minMoveMult) moveMult = minMoveMult;
	float moveSpeed = moveMult / ifs::getCamZoom() * frameDuration;

	if (glm::length2(moveInputs) > 0.0f)
	{
		glm::vec2 deltaPos = glm::rotateZ(glm::vec3(moveInputs.x, moveInputs.y, 0.0f), ifs::getCamAngle()) * moveSpeed;
		ifs::updateCam(deltaPos, pow(2.0f, moveInputs.z * 2.0f * frameDuration), moveInputs.w * glm::pi<float>() * frameDuration);
	}

	double mouseX, mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);
	
	if (firstMouseMovement)
	{
		//set current pos before updating prev pos
		currentMousePos = glm::vec2(mouseX, mouseY);
		prevMousePos = currentMousePos;
		firstMouseMovement = false;
	}
	
	currentMousePos = glm::vec2(mouseX, mouseY);

	glm::vec3 mouseMoveInputs = glm::vec3(0.0f);
	bool shiftHeld = keyMap.at(GLFW_KEY_LEFT_SHIFT).getHeld();
	bool ctrlHeld = keyMap.at(GLFW_KEY_LEFT_CONTROL).getHeld();
	if (keyMap.at(GLFW_MOUSE_BUTTON_LEFT).getHeld())
	{
		if (!(shiftHeld || ctrlHeld)) ifs::updateCamPositionMouse(currentMousePos, prevMousePos);
		if (shiftHeld) ifs::updateCamZoomMouse(currentMousePos, prevMousePos);
		if (ctrlHeld) ifs::updateCamRotationMouse(currentMousePos, prevMousePos);
	}

	if (shiftHeld || ctrlHeld) ifs::enableDrawMouseLine(currentMousePos);

	//update press/hold/release states for next frame
	for (auto it = keyMap.begin(); it != keyMap.end(); it++)
	{
		Key& k = it->second;
		k.updateStates();
	}
	
	prevMousePos = currentMousePos;

	if (showGUI) ifs::createGUI(frameDuration);
	if (firstFrame)
	{
		ImGui::SetWindowFocus(NULL);
		firstFrame = false;
	}

	ifs::update();

	std::chrono::steady_clock::time_point currentFrameEndTime = std::chrono::steady_clock::now();
	frameDuration = (currentFrameEndTime - prevFrameEndTime).count() * 1e-9;
	prevFrameEndTime = currentFrameEndTime;

	return true;
}

void draw()
{
	ifs::draw();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void destroy()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
}

int main()
{
	if (!init()) return -1;

	std::chrono::steady_clock::time_point t0;

	while (!glfwWindowShouldClose(window))
	{
		t0 = std::chrono::steady_clock::now();

		glfwPollEvents();

		if (!update()) break;
		draw();

		glfwSwapBuffers(window);

		if (ifs::getPaused() || ifs::getMaxPreviewFramesReached() || FileRender::getRunning())
		{
			glfwSwapInterval(1);
		}
		else
		{
			glfwSwapInterval(0);
		}
		
	}

	destroy();
}
