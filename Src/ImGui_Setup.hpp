#pragma once

#include <entt.hpp>
#include "Scene.hpp"

class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(Scene& scene) : scene{scene} {}
    void draw(); 

private:
    Scene& scene;
    entt::entity selectedEntity{entt::null};
    void drawInspector();
};