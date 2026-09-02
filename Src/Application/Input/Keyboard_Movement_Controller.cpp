#include "Application/Input/Keyboard_Movement_Controller.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>



void KeyboardMovementController::mouseMove( GLFWwindow *window, float dt, entt::registry &registry, entt::entity entity) {
    auto &transform = registry.get<TransformComponent>(entity);

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (ImGui::GetIO().WantCaptureMouse) {
        // ImGui is interacting with the mouse; skip camera movement
        lastX = xpos;
        lastY = ypos;
        return;
    }

    // Detect middle button "just pressed"
    static bool wasMiddleButtonPressed = false;
    bool isMiddleButtonPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    if (isMiddleButtonPressed && !wasMiddleButtonPressed) {
        // Middle button just pressed, reset lastX/Y to avoid jump
        lastX = xpos;
        lastY = ypos;
    }
    wasMiddleButtonPressed = isMiddleButtonPressed;


    if (rightButtonPressed) {
        float xoffset = xpos - lastX;
        float yoffset = ypos - lastY;

        yaw -= xoffset * lookSpeed * 0.005f;
        pitch -= yoffset * lookSpeed * 0.005f;
    }
    updateRotation(transform);


    // Middle button: pan camera (move in X and Y plane)
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        float xoffset = xpos - lastX;
        float yoffset = ypos - lastY;

        glm::vec3 rightDir{cos(yaw), 0.f, -sin(yaw)};
        glm::vec3 upDir{0.f, 1.f, 0.f};

        transform.translation -= rightDir * (xoffset * moveSpeed * 0.005f);
        transform.translation += upDir * (yoffset * moveSpeed * 0.005f);
    }

    // Scroll wheel
    if (scrollOffset != 0.f) {
        float zoomAmount = scrollOffset * moveSpeed * 0.1f;

        glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
        transform.translation += forwardDir * zoomAmount;

        scrollOffset = 0.f;
    }

    lastX = xpos;
    lastY = ypos;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        rightButtonPressed = true;
    else
        rightButtonPressed = false;
}


void KeyboardMovementController::moveInPlaneXZ(GLFWwindow *window, float dt, entt::registry &registry, entt::entity entity) {
    auto &transform = registry.get<TransformComponent>(entity);

    glm::vec3 rotate{0.f};
    if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
    if (glfwGetKey(window, keys.lookLeft)  == GLFW_PRESS) rotate.y -= 1.f;
    if (glfwGetKey(window, keys.lookUp)    == GLFW_PRESS) rotate.x += 1.f;
    if (glfwGetKey(window, keys.lookDown)  == GLFW_PRESS) rotate.x -= 1.f;

    if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
        yaw += lookSpeed * dt * rotate.y;
        pitch -= lookSpeed * dt * rotate.x;
    }

    updateRotation(transform);

    const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
    const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
    const glm::vec3 upDir{0.f, 1.f, 0.f};

    glm::vec3 moveDir{0.f};
    if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
    if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
    if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
    if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
    if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
    if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

    if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
        transform.translation += moveSpeed * dt * glm::normalize(moveDir);
    }
}


void KeyboardMovementController::scrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset) {
    scrollOffset += static_cast<float>(yoffset);
}

void KeyboardMovementController::bindScrollCallback(GLFWwindow* window) {
    glfwSetScrollCallback(window, KeyboardMovementController::scrollCallback);
}

void KeyboardMovementController::updateRotation(TransformComponent& transform) {
    transform.rotation = glm::angleAxis(yaw, glm::vec3(0.f, 1.f, 0.f))
                        * glm::angleAxis(pitch, glm::vec3(1.f, 0.f, 0.f));
}