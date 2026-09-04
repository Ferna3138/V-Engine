#include "Application/Apps/FirstApp/First_App.hpp"

#include "Rendering/Core/Camera.hpp"
#include "Application/Input/Keyboard_Movement_Controller.hpp"

#include "Rendering/Passes/Simple_Render_System.hpp"
#include "Rendering/Passes/Point_Light_System.hpp"
#include "Rendering/Passes/Depth_Prepass_System.hpp"
#include "Rendering/Passes/Full_Screen_System.hpp"
#include "Rendering/Passes/Post_Fx_Params.hpp"

#include "Rendering/RHI/Buffer.hpp"
#include "Rendering/Resources/Texture.hpp"

#include "Foundation/Platform/AssetPath.hpp"
#include "Application/Scene/Scene_Serializer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include <glm/gtc/type_ptr.hpp>


// Std
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <numeric>

#include <glm/gtc/constants.hpp>


FirstApp::FirstApp() {
    jobSystem.initialize();

    // Pinned to the last worker thread; it blocks in loader->waitForWork() until
    // there's an upload to service, so it costs nothing while idle.
    asyncLoadTask.textureManager = &textureManager;
    asyncLoadTask.loader = &asyncLoader;
    asyncLoadTask.threadNum = jobSystem.scheduler().GetNumTaskThreads() - 1;
    jobSystem.scheduler().AddPinnedTask(&asyncLoadTask);


    globalPool = DescriptorPool::Builder(device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT + 6)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT + 6)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT + 6)
        .build();

    loadGameObjects();

    frameGraph.parse("Src/Application/Apps/FirstApp/FrameGraphs/forward.json");
    frameGraph.compile();
    frameGraph.createResources(device);
    frameGraph.createRenderPasses(device);
    frameGraph.setNodeUsesSecondaryCommandBuffers("forward", true);
    frameGraph.setPresentOutput("ldr_colour");
}

FirstApp::~FirstApp() {
    asyncLoadTask.execute = false;
    asyncLoader.wakeForShutdown();  // unblock waitForWork() so the pinned task can exit
    jobSystem.shutdown();
    // ImGui teardown happens in ~EditorUI (a member, destroyed before `device`).
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


    auto globalSetLayout = DescriptorSetLayout::Builder(device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build();
    
    std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .build(globalDescriptorSets[i]);
    }

    // Camera DoF
    // Filled fresh each frame
    DofDownsampleParams  dofP{};
    DofBlurParams blurP{};
    TonemapParams postP{};
    // Motion Blur
    MotionBlurParams mbP{};
    glm::mat4 prevViewProj{1.f};
    bool haveHistory = false;

    // Geometry is rendered by the frame graph (depth_prepass -> forward), so the
    // render systems build their pipelines against the graph's render passes.
    DepthPrepassSystem depthPrepassSystem{
        device,
        frameGraph.getNodeRenderPass("depth_prepass"),
        globalSetLayout->getDescriptorSetLayout()};
    SimpleRenderSystem simpleRenderSystem{
        device,
        frameGraph.getNodeRenderPass("forward"),
        globalSetLayout->getDescriptorSetLayout(),
        textureManager.getLayout()};
    PointLightSystem pointLightSystem{
        device,
        frameGraph.getNodeRenderPass("forward"),
        globalSetLayout->getDescriptorSetLayout()};
    
    // Full Screen Passes
    FullscreenPass motionBlur{
        device, frameGraph.getNodeRenderPass("motion_blur"), *globalPool,
        "Src/Rendering/Shaders/motion_blur.frag.spv",
        { frameGraph.getResourceImageView("scene_colour"), frameGraph.getResourceImageView("depth") },
        sizeof(MotionBlurParams)
    };

    FullscreenPass dofDownsample{
        device,
        frameGraph.getNodeRenderPass("dof_downsample"),
        *globalPool,
        "Src/Rendering/Shaders/dof_downsample.frag.spv",
        { frameGraph.getResourceImageView("scene_mb"),   // -> binding 0  sceneColour
        frameGraph.getResourceImageView("depth") },        // -> binding 1  sceneDepth
        sizeof(DofDownsampleParams)
    };

    FullscreenPass dofBlur{
        device, 
        frameGraph.getNodeRenderPass("dof_blur"), 
        *globalPool,
        "Src/Rendering/Shaders/dof_blur.frag.spv", 
        {frameGraph.getResourceImageView("dof_far")},
        sizeof(DofBlurParams)
    };
    FullscreenPass post{
        device, 
        frameGraph.getNodeRenderPass("post"), 
        *globalPool,
        "Src/Rendering/Shaders/tonemap.frag.spv",
        {frameGraph.getResourceImageView("scene_mb"), frameGraph.getResourceImageView("dof_far_b")},
        sizeof(TonemapParams)
    };
    


    // The frame graph invokes these inside each pass's render pass, on the primary
    // command buffer. activeFrame is repointed at the current FrameInfo each frame.
    FrameInfo* activeFrame = nullptr;
    frameGraph.registerRenderPass("depth_prepass", [&](VkCommandBuffer cb) {
        FrameInfo fi = *activeFrame; fi.commandBuffer = cb;
        depthPrepassSystem.render(fi);
    });
    
    frameGraph.registerRenderPass("forward", [&](VkCommandBuffer cb) {
        VkRenderPass forwardPass = frameGraph.getNodeRenderPass("forward");
        VkExtent2D extent = frameGraph.getNodeExtent("forward");

        VkCommandBufferInheritanceInfo inheritanceInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
        inheritanceInfo.renderPass = forwardPass;
        inheritanceInfo.subpass = 0;
        inheritanceInfo.framebuffer = VK_NULL_HANDLE;

        VkCommandBufferBeginInfo secBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        secBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        secBeginInfo.pInheritanceInfo = &inheritanceInfo;

        VkViewport viewport{0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, extent};

        VkCommandBuffer meshCB = renderer.getSecondaryCommandBuffer(0);
        VkCommandBuffer lightCB = renderer.getSecondaryCommandBuffer(1);
        FrameInfo fi = *activeFrame;

        enki::TaskSet meshTask(1, [&](enki::TaskSetPartition, uint32_t) {
            vkBeginCommandBuffer(meshCB, &secBeginInfo);
            vkCmdSetViewport(meshCB, 0, 1, &viewport);
            vkCmdSetScissor(meshCB, 0, 1, &scissor);
            FrameInfo meshFi = fi; meshFi.commandBuffer = meshCB;
            simpleRenderSystem.renderGameObjects(meshFi);
            vkEndCommandBuffer(meshCB);
        });
        jobSystem.scheduler().AddTaskSetToPipe(&meshTask);

        enki::TaskSet lightTask(1, [&](enki::TaskSetPartition, uint32_t) {
            vkBeginCommandBuffer(lightCB, &secBeginInfo);
            vkCmdSetViewport(lightCB, 0, 1, &viewport);
            vkCmdSetScissor(lightCB, 0, 1, &scissor);
            if (editorUI.visualisePointLights) {
                FrameInfo lightFi = fi; lightFi.commandBuffer = lightCB;
                pointLightSystem.render(lightFi);
            }
            vkEndCommandBuffer(lightCB);
        });
        jobSystem.scheduler().AddTaskSetToPipe(&lightTask);

        jobSystem.scheduler().WaitforTask(&meshTask);
        jobSystem.scheduler().WaitforTask(&lightTask);

        VkCommandBuffer secondaries[] = { meshCB, lightCB };
        vkCmdExecuteCommands(cb, 2, secondaries);
    });

    frameGraph.registerRenderPass("dof_downsample", [&](VkCommandBuffer cb){dofDownsample.render(cb, &dofP, sizeof(dofP)); });
    frameGraph.registerRenderPass("dof_blur",       [&](VkCommandBuffer cb){ dofBlur.render(cb, &blurP, sizeof(blurP)); });
    frameGraph.registerRenderPass("post",           [&](VkCommandBuffer cb){ post.render(cb, &postP, sizeof(postP)); });
    frameGraph.registerRenderPass("motion_blur",    [&](VkCommandBuffer cb){ motionBlur.render(cb, &mbP, sizeof(mbP)); });



    

    // The scene file creates the camera entity; resolveOrCreateCamera() falls
    // back to a default one. Held by pointer so it can be rebound after a scene
    // reload.
    entt::entity cameraEntity = resolveOrCreateCamera();
    CameraComponent* cameraComponent = &scene.getRegistry().get<CameraComponent>(cameraEntity);
    KeyboardMovementController cameraController{};
    cameraController.syncFromTransform(scene.getRegistry().get<TransformComponent>(cameraEntity));
    // Install our scroll handler once, after ImGui's GLFW callbacks. It forwards to
    // ImGui and only zooms the camera when a panel isn't capturing the wheel.
    cameraController.bindScrollCallback(window.getGLFWwindow());

    RenderScene renderScene;


    // FPS
    auto currentTime = std::chrono::high_resolution_clock::now();

    while (!window.shouldClose()) {
        glfwPollEvents();

        if (editorUI.consumeSceneReloadRequest()) {
            haveHistory = false;
            device.waitIdle();
            scene.getRegistry().clear();
            loadGameObjects();
            cameraEntity = resolveOrCreateCamera();
            cameraComponent = &scene.getRegistry().get<CameraComponent>(cameraEntity);
            cameraController = KeyboardMovementController{};
            cameraController.syncFromTransform(scene.getRegistry().get<TransformComponent>(cameraEntity));
        }

        // Models imported in the background (File > Import Model, drag-and-drop)
        // land here once their GPU upload is done - registry mutation stays on
        // the main thread, same as everything else touching `scene`.
        for (auto& finishedImport : modelImporter.pollFinishedImports()) {
            entt::entity entity = scene.createModelEntity(finishedImport.model, finishedImport.sourcePath);
            std::string meshName = std::filesystem::path(finishedImport.sourcePath).stem().string();
            if (!meshName.empty())
                scene.getRegistry().get<TagComponent>(entity).name = meshName;
            editorUI.selectEntity(entity);
        }

        // Time & FPS
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
        currentTime = newTime;

        // Camera settings
        cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, scene.getRegistry(), cameraEntity);
        cameraController.mouseMove(window.getGLFWwindow(), frameTime, scene.getRegistry(), cameraEntity);

        auto &camTransform = scene.getRegistry().get<TransformComponent>(cameraEntity);
        cameraComponent->camera.setView(camTransform.translation, camTransform.rotation);

        float aspect = renderer.getAspectRatio();

        if(cameraComponent->cameraModel == CameraModel::Physical){
            const auto& cp = cameraComponent->cameraParams;
            // "Auto fit" (like Blender): if the render target is wider than the
            // sensor, the sensor width is the limiting dimension -> derive the
            // horizontal FOV from sensor_width, then the vertical from aspect.
            // Otherwise the sensor height limits, as before.
            float fovy;
            if (aspect >= cp.sensor_width / cp.sensor_height) {
                float fovx = 2.f * atan(cp.sensor_width / (2.f * cp.focal_length));
                fovy = 2.f * atan(tan(fovx * 0.5f) / aspect);
            } else {
                fovy = 2.f * atan(cp.sensor_height / (2.f * cp.focal_length));
            }
            cameraComponent->cameraParams.fov = glm::degrees(fovy);
            cameraComponent->camera.setPerspectiveProjection(fovy, aspect, cp.near_plane, cp.far_plane);
        }else{
            cameraComponent->camera.setPerspectiveProjection(
            glm::radians(cameraComponent->cameraParams.fov), aspect, cameraComponent->cameraParams.near_plane, cameraComponent->cameraParams.far_plane);
        }

        CameraExposure ex = computeExposure(cameraComponent->cameraParams);
        cameraComponent->cameraParams.iso = static_cast<int>(ex.autoIso);

        {
            const auto& p = cameraComponent->cameraParams;
            bool physical = cameraComponent->cameraModel == CameraModel::Physical;
            bool dofOn    = physical && p.dof_enabled;

            dofP.dof     = glm::vec4(p.focal_length, p.aperture, p.focus_distance, p.sensor_height);
            dofP.extra   = glm::vec4(p.near_plane, dofOn ? p.max_coc : 0.f, p.far_plane, 0.f);
            blurP.params = glm::vec4(float(p.aperture_blades), glm::radians(p.blade_rotation),
                                    float(p.dof_samples), 0.f);
            postP.exposure = glm::vec4(ex.whiteBalanceGain, ex.scale);

            glm::mat4 currVP = cameraComponent->camera.getProjection() * cameraComponent->camera.getView();
            mbP.reprojection = haveHistory ? prevViewProj * glm::inverse(currVP) : glm::mat4(1.f);

            // Scale the reconstructed per-frame motion up to the shutter-open
            // interval: blurLength = perFrameMotion * (shutterTime / frameTime).
            float shutterTime = (p.shutter_angle / 360.f) / (p.fps < 1.f ? 1.f : p.fps);
            float mbScale     = (frameTime > 1e-5f) ? shutterTime / frameTime : 0.f;
            mbP.params = glm::vec4(mbScale, p.mb_max_px, float(p.mb_samples),
                       (physical && p.motion_blur_enabled) ? 1.0f : 0.0f);
            prevViewProj = currVP;
            haveHistory = true;
        }

        //printf("exposure=%.4f  wb=(%.2f, %.2f, %.2f)\n", ex.scale, ex.whiteBalanceGain.x, ex.whiteBalanceGain.y, ex.whiteBalanceGain.z);

        scene.buildRenderScene(renderScene);

        if (auto commandBuffer = renderer.beginFrame()) {
            int frameIndex = renderer.getFrameIndex();

            FrameInfo frameInfo{
                frameIndex,
                frameTime,
                commandBuffer,
                cameraComponent->camera,
                globalDescriptorSets[frameIndex],
                textureManager.getDescriptorSet(),
                renderScene
            };

            // Update
            GlobalUbo ubo{};
            ubo.projection = cameraComponent->camera.getProjection();
            ubo.view = cameraComponent->camera.getView();
            ubo.inverseView = cameraComponent->camera.getInverseView();
            ubo.inverseProj = cameraComponent->camera.getInverseProj();
            

            pointLightSystem.update(frameInfo, ubo);
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();

            editorUI.draw();
            frameGraph.setClearColour(editorUI.backgroundColour.r,
                                      editorUI.backgroundColour.g,
                                      editorUI.backgroundColour.b);

            // Frame graph: depth pre-pass -> forward, into an offscreen image,
            //with barriers auto-inserted from the graph edges.
            activeFrame = &frameInfo;
            frameGraph.execute(commandBuffer);

            // Composite: blit the graph's colour output into the acquired
            // swapchain image, then draw the UI on top.
            FrameGraph::OutputImage sceneColour = frameGraph.getPresentOutput();
            VkImage swapImage = renderer.getCurrentSwapChainImage();
            VkExtent2D swapChainExtent = renderer.getSwapChainExtent();

            VkImageMemoryBarrier toDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toDst.image = swapImage;
            toDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            toDst.srcAccessMask = 0;
            toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toDst);

            VkImageBlit blit{};
            blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.srcOffsets[1] = { (int32_t)sceneColour.width, (int32_t)sceneColour.height, 1 };
            blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.dstOffsets[1] = { (int32_t)swapChainExtent.width, (int32_t)swapChainExtent.height, 1 };
            vkCmdBlitImage(commandBuffer,
                sceneColour.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);

            VkImageMemoryBarrier toColour{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toColour.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toColour.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            toColour.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toColour.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toColour.image = swapImage;
            toColour.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            toColour.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toColour.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toColour);

            renderer.beginSwapChainRenderPass(commandBuffer);

            VkCommandBufferInheritanceInfo inheritanceInfo{};
            inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
            inheritanceInfo.renderPass = renderer.getSwapChainRenderPass();
            inheritanceInfo.subpass = 0;
            inheritanceInfo.framebuffer = VK_NULL_HANDLE;

            VkCommandBufferBeginInfo secBeginInfo{};
            secBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            secBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            secBeginInfo.pInheritanceInfo = &inheritanceInfo;

            VkCommandBuffer imguiCB = renderer.getSecondaryCommandBuffer(2);
            vkBeginCommandBuffer(imguiCB, &secBeginInfo);
            {
                //  uploads dirty font/texture
                // atlases with its own vkQueueSubmit + vkQueueWaitIdle on the
                // graphics queue, so it must be serialised against the async
                // loader thread's queue submissions.
                std::lock_guard<std::mutex> lock(device.queueMutex);
                editorUI.recordDrawData(imguiCB);
            }
            vkEndCommandBuffer(imguiCB);
            vkCmdExecuteCommands(commandBuffer, 1, &imguiCB);

            renderer.endSwapChainRenderPass(commandBuffer);
            renderer.endFrame();
        }
    }

    device.waitIdle();
}




void FirstApp::loadGameObjects() {
    if (!vengine::loadScene(scenePath, scene, device, textureManager)) {
        std::cerr << "Failed to load " << scenePath << " - starting with an empty scene\n";
    }
}

entt::entity FirstApp::resolveOrCreateCamera() {
    auto& registry = scene.getRegistry();
    for (auto entity : registry.view<CameraComponent>())
        return entity;

    entt::entity camera = scene.createEntity("Camera");
    registry.emplace<CameraComponent>(camera);
    registry.get<TransformComponent>(camera).translation = {0.f, 1.5f, 0.5f};
    return camera;
}
