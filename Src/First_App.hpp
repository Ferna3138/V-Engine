#pragma once
#include "Renderer/Device.hpp"
#include "Renderer/Pipeline.hpp"
#include "Renderer/Window.hpp"
#include "Components/Game_Object.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Descriptors.hpp"

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
        void loadGameObjects();

        Window window{WIDTH, HEIGHT, "V-Engine"};
        Device device{window};
        Renderer renderer{window, device};

        // Note: Order of declarations matters
        std::unique_ptr<DescriptorPool> globalPool;
        std::vector<GameObject> gameObjects;
  };