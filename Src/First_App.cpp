#include "First_App.hpp"

#include "Systems/Camera.hpp"
#include "Systems/Keyboard_Movement_Controller.hpp"
#include "Render_Systems/Simple_Render_System.hpp"
#include "Render_Systems/Point_Light_System.hpp"
#include "Renderer/Buffer.hpp"
#include "Components/Texture.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include <glm/gtc/type_ptr.hpp>


// Std
#include <array>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <numeric>

#include <glm/gtc/constants.hpp>



FirstApp::FirstApp() {
    globalPool = DescriptorPool::Builder(device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .build();

    loadGameObjects();
}

FirstApp::~FirstApp() {
    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);

    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device.device(), imguiPool, nullptr);
}


void FirstApp::run() {

    std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        uboBuffers[i] = std::make_unique<Buffer>(
            device,
            sizeof(GlobalUbo),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uboBuffers[i]->map(); 
    }

    Texture texture = Texture(device, "../Models/Sponza_obj/textures/sponza_ceiling_a_diff.tga");

    VkDescriptorImageInfo imageInfo {};
    imageInfo.sampler = texture.getSampler();
    imageInfo.imageView = texture.getImageView();
    imageInfo.imageLayout = texture.getImageLayout();

    auto globalSetLayout = DescriptorSetLayout::Builder(device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build();
    
    std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &imageInfo)
            .build(globalDescriptorSets[i]);
    }


    // Pipeline creation
    SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
    PointLightSystem pointLightSystem{device, renderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};

    

    setUpImgui();

    Camera camera{};
    camera.setViewTarget(glm::vec3(0.f, 0.f, -2.f), glm::vec3(0.f, 0.f, 2.5f));

    auto viewerObject = GameObject::createGameObject();
    viewerObject.transform.translation.z = 0.5f;
    KeyboardMovementController cameraController{};
    auto currentTime = std::chrono::high_resolution_clock::now();

    while (!window.shouldClose()) {
        glfwPollEvents();

        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;

        cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, viewerObject);
        
        // Handle mouse movement
        cameraController.mouseMove(window.getGLFWwindow(), frameTime, viewerObject);
        cameraController.bindScrollCallback(window.getGLFWwindow());

        camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

        float aspect = renderer.getAspectRatio();
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

        if (auto commandBuffer = renderer.beginFrame()) {
            int frameIndex = renderer.getFrameIndex();
            
            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                camera,
                globalDescriptorSets[frameIndex],
                scene.getRegistry()
            };

            // Update
            GlobalUbo ubo{};
            ubo.projection = camera.getProjection();
            ubo.view = camera.getView();
            ubo.inverseView = camera.getInverseView();
            
            pointLightSystem.update(frameInfo, ubo);
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();
            
            renderUI();

            // Render
            renderer.beginSwapChainRenderPass(commandBuffer);
             
            // Order here matters
            simpleRenderSystem.renderGameObjects(frameInfo);

            if(visualisePointLights)
                pointLightSystem.render(frameInfo);

            // Render ImGui Frame
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);


            renderer.endSwapChainRenderPass(commandBuffer);
            renderer.endFrame();
        }
    }

    vkDeviceWaitIdle(device.device());
    ImGui::SaveIniSettingsToDisk("imgui.ini");

}




void FirstApp::loadGameObjects() {
    std::shared_ptr<Model> model = Model::createModelFromFile(device, "../Models/Sponza_obj/sponza.obj");



    entt::entity sponza = scene.createModelEntity(model);
    auto& transform = scene.getRegistry().get<TransformComponent>(sponza);
    transform.translation = {0.0f, 2.0f, 0.0f};
    transform.scale = glm::vec3{-0.01f, -0.01f, -0.01f};


    std::vector<glm::vec3> lightColors{
        {0.956f, 0.262f, 0.211f},  // Soft red (sunset red)
        {0.129f, 0.588f, 0.953f},  // Clear blue (sky/cool fill light)
        {0.298f, 0.686f, 0.314f},  // Natural green (forest bounce)
        {1.000f, 0.839f, 0.000f},  // Warm yellow (torch/firelight)
        {0.616f, 0.153f, 0.690f},  // Lavender (magic/night light)
        {0.933f, 0.910f, 0.667f}   // Pale warm white (keylight / candle)
    };
    

    /*
    for (int i = 0; i < lightColors.size(); i++) {
        entt::entity pointLight = scene.createPointLight(0.2f);
        auto rotateLight = glm::rotate(glm::mat4(1.f), (i * glm::two_pi<float>() / lightColors.size()), glm::vec3(0.f, -1.f, 0.f));
        auto& transform = scene.getRegistry().get<TransformComponent>(pointLight);
        transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
 
        auto& light = scene.getRegistry().get<PointLightComponent>(pointLight);
    }*/

//    mainLight = scene.createPointLight(glm::vec4(1.0f, 1.0f, 1.0f, 10.0f), 0.2f);
//    auto& lightTransform = scene.getRegistry().get<TransformComponent>(mainLight);
//    lightTransform.translation = {0.0f, -5.0f, 0.0f};


    for (size_t i = 0; i < lightColors.size(); i++) {
        entt::entity light = scene.createPointLight(
            glm::vec4(lightColors[i], 3.0f), // RGB + intensity in .w
            0.2f
        );
        auto& lightTransform = scene.getRegistry().get<TransformComponent>(light);
        lightTransform.translation = {static_cast<float>(i) * 2.0f, -5.0f, 0.0f};
    }

}



void FirstApp::renderUI() {
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_NoBackground;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();

    // === Control Panel ===
    ImGui::Begin("Control Panel");

    ImGui::Checkbox("Visualise Point Lights", &visualisePointLights);

    // Cache lights in a list
    static int selectedLightIndex = 0;
    auto view = scene.getRegistry().view<TransformComponent, PointLightComponent>();
    
    std::size_t count = static_cast<std::size_t>(std::distance(view.begin(), view.end()));

    std::vector<entt::entity> lightEntities;
    lightEntities.clear();
    lightEntities.reserve(count);

    for (auto entity : view) {
        lightEntities.push_back(entity);
    }

    if (!lightEntities.empty()) {
        // Keep index valid if lights change
        if (selectedLightIndex >= static_cast<int>(lightEntities.size())) {
            selectedLightIndex = static_cast<int>(lightEntities.size()) - 1;
        }

        // Light selection combo
        std::string previewName = "Light " + std::to_string(selectedLightIndex);
        if (ImGui::BeginCombo("Select Light", previewName.c_str())) {
            for (int i = 0; i < static_cast<int>(lightEntities.size()); i++) {
                bool isSelected = (selectedLightIndex == i);
                std::string name = "Light " + std::to_string(i);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    selectedLightIndex = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Draw controls for selected light
        entt::entity selectedEntity = lightEntities[selectedLightIndex];
        auto &transform = view.get<TransformComponent>(selectedEntity);
        auto &light = view.get<PointLightComponent>(selectedEntity);

        ImGui::Separator();
        ImGui::Text("Editing Light %d", selectedLightIndex);
        ImGui::SliderFloat3("Position", &transform.translation.x, -10.0f, 10.0f);
        ImGui::SliderFloat("Intensity", &light.colour.w, 0.0f, 50.0f);
        ImGui::ColorEdit3("Color", &light.colour.x);
    } else {
        ImGui::Text("No point lights in scene.");
    }

    ImGui::End();


    ImGui::Render();
}

void FirstApp::setUpImgui() {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(device.device(), &pool_info, nullptr, &imguiPool);

    // --- ImGui Init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::GetIO().IniFilename = "imgui.ini"; // Optional, defaults to this


    // Enable Docking + Viewports
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Optional
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 1.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = device.getInstance();
    init_info.PhysicalDevice = device.getPhysicalDevice();
    init_info.Device = device.device();
    init_info.Queue = device.graphicsQueue();
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = SwapChain::MAX_FRAMES_IN_FLIGHT;
    init_info.UseDynamicRendering = false;
    init_info.RenderPass = renderer.getSwapChainRenderPass();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}