#pragma once
#include "Renderer/Device.hpp"
#include "Renderer/Pipeline.hpp"
#include "Renderer/Window.hpp"
#include "Components/Game_Object.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Descriptors.hpp"


// ImGui
#include <imgui/imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>


// Std
#include <memory>
#include <vector>

class FirstApp {
    public:
        static constexpr int WIDTH = 800;
        static constexpr int HEIGHT = 600;

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

        void loadGameObjects();

        Window window{WIDTH, HEIGHT, "V-Engine"};
        Device device{window};
        Renderer renderer{window, device};

        // Note: Order of declarations matters
        std::unique_ptr<DescriptorPool> globalPool;

        GameObject::Map gameObjects;

};
