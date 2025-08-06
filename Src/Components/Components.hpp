#pragma once

#include <glm/glm.hpp>

struct TransformComponent{
        glm::vec3 translation{};
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::vec3 rotation{0.f}; // in radians
        
        glm::mat4 mat4();
        glm::mat3 normalMatrix();
};

struct ModelComponent{
    std::shared_ptr<Model> model{};
};

