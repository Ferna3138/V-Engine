#include "First_App.hpp"

#include "Systems/Camera.hpp"
#include "Systems/Keyboard_Movement_Controller.hpp"
#include "Simple_Render_System.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

// Std
#include <array>
#include <cassert>
#include <chrono>
#include <stdexcept>

#include <glm/gtc/constants.hpp>



FirstApp::FirstApp() {
    loadGameObjects();
}

FirstApp::~FirstApp() {}

void FirstApp::run() {
    SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapChainRenderPass()};
    Camera camera{};
    camera.setViewTarget(glm::vec3(0.f, 0.f, -2.f), glm::vec3(0.f, 0.f, 2.5f));

    auto viewerObject = GameObject::createGameObject();
    KeyboardMovementController cameraController{};

    auto currentTime = std::chrono::high_resolution_clock::now();

    while (!window.shouldClose()) {
        glfwPollEvents();

        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;

        cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, viewerObject);
        
        // Handle mouse movement
        cameraController.mouseMove(window.getGLFWwindow(), frameTime, viewerObject);
        cameraController.bindScrollCallback(window.getGLFWwindow());

        camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        float aspect = renderer.getAspectRatio();
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

        if (auto commandBuffer = renderer.beginFrame()) {
            renderer.beginSwapChainRenderPass(commandBuffer);

            simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera);

            renderer.endSwapChainRenderPass(commandBuffer);
            renderer.endFrame();
        }
    }

    vkDeviceWaitIdle(device.device());
}




void FirstApp::loadGameObjects() {
    std::shared_ptr<Model> model = Model::createModelFromFile(device, "Models/flat_vase.obj");

    auto flatVase = GameObject::createGameObject();
    flatVase.model = model;
    flatVase.transform.translation = {-0.5f, 0.5f, 2.5f};
    flatVase.transform.scale = glm::vec3{3.f, 1.5f, 3.f};

    gameObjects.push_back(std::move(flatVase));


    model = Model::createModelFromFile(device, "Models/smooth_vase.obj");

    auto smoothVase = GameObject::createGameObject();
    smoothVase.model = model;
    smoothVase.transform.translation = {0.5f, 0.5f, 2.5f};
    smoothVase.transform.scale = glm::vec3{3.f, 1.5f, 3.f};

    gameObjects.push_back(std::move(smoothVase));
}