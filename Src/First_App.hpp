#pragma once
#include "Renderer/Device.hpp"
#include "Renderer/Pipeline.hpp"
#include "Renderer/Window.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Descriptors.hpp"
#include "Renderer/TextureManager.hpp"
#include "Renderer/Frame_Graph.hpp"
#include "ImGui_Setup.hpp"

#include "Scene.hpp"
#include "TaskScheduler.h"
#include "Renderer/Async_Loader.hpp"

// ImGui
//#define IMGUI_ENABLE_DOCKING
#include <imgui/imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>


// Std
#include <memory>
#include <vector>
#include <iostream>
#include <string>

struct IOThreadLoopTask : enki::IPinnedTask {
    void Execute() override {
        while (!taskScheduler->GetIsShutdownRequested()) {
            taskScheduler->WaitForNewPinnedTasks();
            taskScheduler->RunPinnedTasks();
        }
    }
    enki::TaskScheduler* taskScheduler = nullptr;
};

struct AsyncLoadTask : enki::IPinnedTask {
    void Execute() override {
        while (execute.load()) {
            textureManager->update();
        }
    }
    TextureManager* textureManager = nullptr;
    std::atomic<bool> execute{true};
};

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
        // Backs ImGuiIO::IniFilename (ImGui keeps the pointer, so this must
        // outlive the ImGui context). Empty appName -> shared default imgui.ini.
        std::string imguiIniFilePath;
        bool visualisePointLights = true;
        
        void loadGameObjects();

        enki::TaskScheduler taskScheduler;
        
        Window window{WIDTH, HEIGHT, "V-Engine"};
        Device device{window};
        Renderer renderer{window, device};
        AsyncLoader asyncLoader = AsyncLoader(device);
        TextureManager textureManager{device, asyncLoader};
        FrameGraph frameGraph;
        
        IOThreadLoopTask ioThreadLoopTask;
        AsyncLoadTask asyncLoadTask;

        // Note: Order of declarations matters
        std::unique_ptr<DescriptorPool> globalPool;
        
        Scene scene;
        SceneHierarchyPanel sceneHierarchyPanel{scene};
};
