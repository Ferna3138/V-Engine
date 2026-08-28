#pragma once


// Libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

struct CamParameters{
    glm::vec3 position = {0.f,0.f,0.f};
    uint32_t sendor_width = 0;
    uint32_t sendor_height = 0;
    float focal_length = 0.f;
    float fov = 0.f;
    float near_plane = 0.f;
    float far_plane = 0.f;
    float aspect_ratio = 0.f;
    float aperture = 0.f;
    float focus_distance = 0.f;
    float shutter_speed = 0.f;
    float exposure = 0.f;
    uint32_t white_balance = 0.f;
};

class Camera{
    public:
        Camera() = default;

        void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
        void setPerspectiveProjection(float fovy, float aspectRatio, float near, float far);

        void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
        void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
        void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

        const glm::mat4& getProjection() const { return projectionMatrix; }
        const glm::mat4& getView() const { return viewMatrix; }
        const glm::mat4& getInverseView() const { return inverseViewMatrix; } 
        const glm::mat4& getInverseProj() const { return inverseProjectionMatrix; } 

        const glm::vec3 getPosition() const { return glm::vec3(inverseViewMatrix[3]); }
    private:
        glm::mat4 projectionMatrix{1.f};
        glm::mat4 viewMatrix{1.f};
        glm::mat4 inverseViewMatrix{1.f};
        glm::mat4 inverseProjectionMatrix{1.f};
};
