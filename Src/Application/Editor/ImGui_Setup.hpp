#pragma once

#include <entt.hpp>
#include "Application/Scene/Scene.hpp"

class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(Scene& scene) : scene{scene} {}
    void draw(); 

private:
    Scene& scene;
    entt::entity selectedEntity{entt::null};
    entt::entity entityToDelete{entt::null};

    entt::entity cachedRotationEntity{entt::null};
    glm::vec3 editedEulerDegrees{0.f};
    void drawInspector();
};