#pragma once
#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Pipeline.hpp"
#include "Foundation/Platform/Window.hpp"
#include "Rendering/Core/Renderer.hpp"
#include "Rendering/RHI/Descriptors.hpp"
#include "Rendering/Resources/TextureManager.hpp"
#include "Rendering/FrameGraph/Frame_Graph.hpp"
#include "Application/Editor/ImGui_Setup.hpp"

#include "Application/Scene/Scene.hpp"
#include "Foundation/Jobs/JobSystem.hpp"
#include "Rendering/Resources/Async_Loader.hpp"

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

// Runs for the whole app lifetime, pinned to a dedicated enkiTS worker thread.
// enkiTS internal threads already pump pinned tasks in their own loop, so no
// separate "run pinned tasks" driver task is needed.
struct AsyncLoadTask : enki::IPinnedTask {
    void Execute() override {
        while (execute.load(std::memory_order_relaxed)) {
            loader->waitForWork();  // blocks until there's an upload to service or shutdown
            if (!execute.load(std::memory_order_relaxed)) break;
            textureManager->update();
        }
    }
    TextureManager* textureManager = nullptr;
    AsyncLoader* loader = nullptr;
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
        void drawMainMenuBar();
        VkDescriptorPool imguiPool;
        // Backs ImGuiIO::IniFilename (ImGui keeps the pointer, so this must
        // outlive the ImGui context).
        std::string imguiIniFilePath;
        bool visualisePointLights = true;

        void loadGameObjects();
        entt::entity resolveOrCreateCamera();

        std::string scenePath{"Scenes/sponza.json"};
        bool sceneReloadRequested = false;

        vengine::JobSystem jobSystem;

        Window window{WIDTH, HEIGHT, "V-Engine"};
        Device device{window};
        Renderer renderer{window, device};
        AsyncLoader asyncLoader = AsyncLoader(device);
        TextureManager textureManager{device, asyncLoader, jobSystem.scheduler()};
        FrameGraph frameGraph;
        
        AsyncLoadTask asyncLoadTask;

        // Note: Order of declarations matters
        std::unique_ptr<DescriptorPool> globalPool;
        
        Scene scene;
        SceneHierarchyPanel sceneHierarchyPanel{scene};
};
