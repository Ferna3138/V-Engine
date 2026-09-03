#pragma once


#include "imgui.h"
#include <entt.hpp>
#include "Application/Scene/Components.hpp"
#include "Foundation/Platform/Window.hpp"

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

    // Adopt the orientation currently held by a transform (loaded scene, inspector
    // edit) so the controller doesn't clobber it. Call whenever (re)binding a camera.
    void syncFromTransform(const TransformComponent &transform);

    void bindScrollCallback(GLFWwindow *window);

    KeyMappings keys{};
    float moveSpeed {3.f};
    float lookSpeed {1.5f};

private:
    bool rightButtonPressed = false;
    bool middleButtonPressed = false;
    double lastX = 0.0;
    double lastY = 0.0;

    float yaw = 0.f;
    float pitch = 0.f;

    // Last rotation this controller wrote. If the transform differs from it at the
    // start of a frame, something else changed it (inspector, gizmo) -> re-adopt.
    glm::quat lastApplied{1.f, 0.f, 0.f, 0.f};
    void adoptExternalRotation(const TransformComponent &transform);

    inline static float scrollOffset = 0.f;
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    void updateRotation(TransformComponent& transform);
};