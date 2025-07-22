#pragma once
#include "Device.hpp"
#include "Pipeline.hpp"
#include "Window.hpp"
#include "Game_Object.hpp"
#include "Renderer.hpp"

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
        std::vector<GameObject> gameObjects;
  };