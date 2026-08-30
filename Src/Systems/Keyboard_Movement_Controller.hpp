#pragma once


#include "imgui.h"
#include <entt.hpp>
#include "Components/Components.hpp"
#include "Renderer/Window.hpp"

class KeyboardMovementController {
public:
    struct KeyMappings {
        int moveLeft = GLFW_KEY_A;
        int moveRight = GLFW_KEY_D;
        int moveForward = GLFW_KEY_W;
        int moveBackward = GLFW_KEY_S;
        int moveUp = GLFW_KEY_E;
        int moveDown = GLFW_KEY_Q;
        int lookLeft = GLFW_KEY_LEFT;
        int lookRight = GLFW_KEY_RIGHT;
        int lookUp = GLFW_KEY_UP;
        int lookDown = GLFW_KEY_DOWN;
    };

    void moveInPlaneXZ(GLFWwindow *window, float dt, entt::registry &registry, entt::entity entity);
    void mouseMove(GLFWwindow *window, float dt, entt::registry &registry, entt::entity entity);

    void bindScrollCallback(GLFWwindow *window);

    KeyMappings keys{};
    float moveSpeed {3.f};
    float lookSpeed {1.5f};

private:
    bool leftButtonPressed = false;
    bool rightButtonPressed = false;
    double lastX = 0.0;
    double lastY = 0.0;

    float yaw = 0.f;
    float pitch = 0.f;

    inline static float scrollOffset = 0.f;
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    void updateRotation(TransformComponent& transform);
};