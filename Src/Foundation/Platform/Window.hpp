#pragma once
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <vector>

class Window {
    public:
        using DropCallback = std::function<void(const std::vector<std::string>&)>;

        Window(int w, int h, std::string name);
        ~Window();

        Window(const Window&) = delete;
        Window &operator = (const Window &) = delete;

        bool shouldClose() { return glfwWindowShouldClose(window); }
        VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
        bool wasWindowResized() {return framebufferResized;}
        void resetWindowResizedFlag() {framebufferResized = false;}
        GLFWwindow *getGLFWwindow() const { return window; }

        void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

        // Invoked with the dropped file paths whenever the user drags files
        // onto the window. Replaces any previously bound callback.
        void setDropCallback(DropCallback callback);

    private:
        static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
        static void fileDropCallback(GLFWwindow *window, int count, const char **paths);
        void initWindow();

        int width;
        int height;
        bool framebufferResized = false;

        std::string windowName;
        GLFWwindow *window;
        DropCallback dropCallback;

};