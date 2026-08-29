#pragma once
#include "Renderer/Device.hpp"
#include "Renderer/Pipeline.hpp"
#include "Renderer/Window.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Descriptors.hpp"
#include "Renderer/TextureManager.hpp"
#include "ImGui_Setup.hpp"

#include "Scene.hpp"

// ImGui
#define IMGUI_ENABLE_DOCKING
#include <imgui/imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>


// Std
#include <memory>
#include <vector>

class FirstApp {
    public:
        static constexpr int WIDTH = 1200;
        static constexpr int HEIGHT = 800;

        FirstApp();
        ~FirstApp();

        FirstApp(const FirstApp &) = delete;
        FirstApp &operator=(const FirstApp &) = delete;

        void run();


    private:
        // ImGui
        void setUpImgui();
        void renderUI();
        VkDescriptorPool imguiPool;
        bool visualisePointLights = true;
        
        void loadGameObjects();
        
        Window window{WIDTH, HEIGHT, "V-Engine"};
        Device device{window};
        Renderer renderer{window, device};
        TextureManager textureManager{device};
        
        // Note: Order of declarations matters
        std::unique_ptr<DescriptorPool> globalPool;
        
        Scene scene;
        SceneHierarchyPanel sceneHierarchyPanel{scene};
};
