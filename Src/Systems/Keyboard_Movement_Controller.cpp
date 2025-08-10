#include "Keyboard_Movement_Controller.hpp"

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

    if (leftButtonPressed) {
        float xoffset = xpos - lastX;
        float yoffset = ypos - lastY;

        transform.rotation.y -= xoffset * lookSpeed * dt * 0.05f;
        transform.rotation.x += yoffset * lookSpeed * dt * 0.05f;
    }

    lastX = xpos;
    lastY = ypos;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        leftButtonPressed = true;
    else
        leftButtonPressed = false;


    
    
}


void KeyboardMovementController::moveInPlaneXZ(GLFWwindow *window, float dt, entt::registry &registry, entt::entity entity) {
    auto &transform = registry.get<TransformComponent>(entity);

    glm::vec3 rotate{0.f};
    if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
    if (glfwGetKey(window, keys.lookLeft)  == GLFW_PRESS) rotate.y -= 1.f;
    if (glfwGetKey(window, keys.lookUp)    == GLFW_PRESS) rotate.x += 1.f;
    if (glfwGetKey(window, keys.lookDown)  == GLFW_PRESS) rotate.x -= 1.f;

    if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
        transform.rotation += lookSpeed * dt * glm::normalize(rotate);
    }

    float yaw = transform.rotation.y;
    const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
    const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
    const glm::vec3 upDir{0.f, -1.f, 0.f};

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