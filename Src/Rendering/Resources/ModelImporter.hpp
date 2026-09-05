#pragma once

#include "Rendering/RHI/Device.hpp"
#include "Rendering/Resources/MaterialManager.hpp"
#include "Rendering/Resources/Model.hpp"
#include "Rendering/Resources/TextureManager.hpp"

#include "TaskScheduler.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Loads a model file off the main thread: file parse, material textures, and
// GPU vertex/index buffer upload all happen on an enkiTS worker task. Backs
// hot model import (File > Import Model, drag-and-drop) without stalling the
// render loop. Registry mutation is still the caller's job - it must be done
// on the main thread, so pollFinishedImports() only hands back fully
// GPU-resident models for the caller to turn into ModelComponents.
class ModelImporter {
public:
    struct FinishedImport {
        std::string sourcePath;
        std::shared_ptr<Model> model;
    };

    ModelImporter(Device& device, TextureManager& textureManager, MaterialManager& materialManager,
                  enki::TaskScheduler& taskScheduler);

    // Queues a background import and returns immediately. Safe to call from
    // the main thread (menu item) or a GLFW callback (drag-and-drop).
    void requestImport(const std::string& filepath);

    // Drains models that finished loading since the last call. Call once per
    // frame on the main thread and hand each result to Scene::createModelEntity.
    std::vector<FinishedImport> pollFinishedImports();

private:
    struct ImportTask : enki::ITaskSet {
        ModelImporter* owner = nullptr;
        std::string filepath;
        void ExecuteRange(enki::TaskSetPartition, uint32_t) override;
    };

    Device& device;
    TextureManager& textureManager;
    MaterialManager& materialManager;
    enki::TaskScheduler& taskScheduler;

    // Self-owned, kept alive for the app's lifetime (mirrors
    // TextureManager::decodeTasks) so each task object outlives its execution.
    // Only touched from the main thread (requestImport), so no lock needed.
    std::vector<std::unique_ptr<ImportTask>> tasks;

    std::mutex finishedMutex;
    std::vector<FinishedImport> finished;  // guarded by finishedMutex
};
