#include "Keyboard_Movement_Controller.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

void KeyboardMovementController::mouseMove(GLFWwindow* window, float dt, GameObject& gameObject) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    int stateLeft = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    int stateRight = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);

    double dx = xpos - lastX;
    double dy = ypos - lastY;

    // Static quaternion to hold camera orientation
    static glm::quat orientation = glm::quat(glm::vec3(
        gameObject.transform.rotation.x,
        gameObject.transform.rotation.y,
        gameObject.transform.rotation.z
    ));

    // Left click = orbit (rotate camera orientation)
    if (stateLeft == GLFW_PRESS) {
        float yaw = -static_cast<float>(dx) * 0.002f;
        float pitch = -static_cast<float>(dy) * 0.002f;

        glm::quat qPitch = glm::angleAxis(pitch, glm::vec3(1.f, 0.f, 0.f));
        glm::quat qYaw   = glm::angleAxis(yaw,   glm::vec3(0.f, 1.f, 0.f));

        orientation = glm::normalize(qYaw * orientation * qPitch);
    }

    // Extract local axes from quaternion
    glm::mat3 rotMatrix = glm::mat3_cast(orientation);
    glm::vec3 forward = -rotMatrix[2];  // negative Z
    glm::vec3 right   =  rotMatrix[0];  // positive X
    glm::vec3 up      =  rotMatrix[1];  // positive Y

    // Right click = pan along right and up axes
    if (stateRight == GLFW_PRESS) {
        float deltaSpeed = moveSpeed * dt * 0.1f;
        gameObject.transform.translation += static_cast<float>(-dx) * right * deltaSpeed;
        gameObject.transform.translation += static_cast<float>(dy) * up * deltaSpeed;
    }

    // Scroll = move along forward axis
    if (std::abs(scrollOffset) > std::numeric_limits<float>::epsilon()) {
        gameObject.transform.translation += scrollOffset * forward * dt * moveSpeed;
        scrollOffset = 0.f;
    }

    // Store final orientation back as Euler for consistency with the rest of your engine
    glm::vec3 euler = glm::eulerAngles(orientation);
    gameObject.transform.rotation.x = euler.x;
    gameObject.transform.rotation.y = euler.y;
    gameObject.transform.rotation.z = euler.z;

    lastX = xpos;
    lastY = ypos;
}


void KeyboardMovementController::moveInPlaneXZ(GLFWwindow *window, float dt, GameObject &gameObject) {
    glm::vec3 rotate{0};

    if(glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
    if(glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
    if(glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
    if(glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

    if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
        gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);    
    }

    gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
    gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

    float yaw = gameObject.transform.rotation.y;
    const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
    const glm::vec3 rightDir(forwardDir.z, 0.f, -forwardDir.x);
    const glm::vec3 upDir(0.f, -1.f, 0.f);

    glm::vec3 moveDir{0.f};
    if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
    if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
    if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
    if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
    if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
    if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

    if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
        gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);    
    }
}


void KeyboardMovementController::scrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset) {
    scrollOffset += static_cast<float>(yoffset);
}

void KeyboardMovementController::bindScrollCallback(GLFWwindow* window) {
    glfwSetScrollCallback(window, KeyboardMovementController::scrollCallback);
}