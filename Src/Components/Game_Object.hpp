#pragma once

#include "Components.hpp"
#include "Model.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <unordered_map>


struct PointLightComponent {
    float lightIntensity = 1.0f; // Intensity of the light
};

class GameObject {
    

    public:
        using id_t = unsigned int;
        using Map = std::unordered_map<id_t, GameObject>;

        static GameObject createGameObject() {
            static id_t currentId = 0;
            return GameObject{currentId++};
        }

        static GameObject makePointLight(float intensity = 10.f, float radius = 0.1f, glm::vec3 colour = glm::vec3(1.f));

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        GameObject(GameObject&&) = default;
        GameObject& operator=(GameObject&&) = default;

        const id_t getId() const { return id; }

        glm::vec3 colour{};
        TransformComponent transform{};
        
        std::shared_ptr<Model> model{};
        std::unique_ptr<PointLightComponent> pointLight = nullptr;

    private:
        GameObject(id_t objId) : id{objId} {}
        
        id_t id;
};

