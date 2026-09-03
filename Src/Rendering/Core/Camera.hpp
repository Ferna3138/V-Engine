#pragma once


// Libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

enum class CameraModel { Pinhole, Physical };


struct CamParameters{
    glm::vec3 position = {0.f,0.f,0.f};
    float sensor_width = 36.f;
    float sensor_height = 24.f;
    float focal_length = 24.f;
    float fov = 50.f;
    float near_plane = 0.1f;
    float far_plane = 100.f;
    float aspect_ratio = 0.f;
    float aperture = 2.f;
    float focus_distance = 2.f;
    float shutter_angle = 180.f;
    float exposure = 0.f;
    int white_balance = 6500;
    int iso = 100;
    float fps = 60.f;
};

class Camera{
    public:
        Camera() = default;

        void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
        void setPerspectiveProjection(float fovy, float aspectRatio, float near, float far);

        void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, 1.f, 0.f});
        void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3{0.f, 1.f, 0.f});
        void setView(glm::vec3 position, glm::quat rotation);

        const glm::mat4& getProjection() const { return projectionMatrix; }
        const glm::mat4& getView() const { return viewMatrix; }
        const glm::mat4& getInverseView() const { return inverseViewMatrix; } 
        const glm::mat4& getInverseProj() const { return inverseProjectionMatrix; } 

        const glm::vec3 getPosition() const { return glm::vec3(inverseViewMatrix[3]); }
    private:
        
        CameraModel cameraModel = CameraModel::Pinhole;
        glm::mat4 projectionMatrix{1.f};
        glm::mat4 viewMatrix{1.f};
        glm::mat4 inverseViewMatrix{1.f};
        glm::mat4 inverseProjectionMatrix{1.f};
};
