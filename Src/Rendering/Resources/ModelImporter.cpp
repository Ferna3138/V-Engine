#include "Rendering/Resources/ModelImporter.hpp"

#include <cstdio>
#include <stdexcept>

ModelImporter::ModelImporter(Device& _device, TextureManager& _textureManager, MaterialManager& _materialManager,
                              enki::TaskScheduler& _taskScheduler)
    : device{_device}, textureManager{_textureManager}, materialManager{_materialManager}, taskScheduler{_taskScheduler} {}

void ModelImporter::requestImport(const std::string& filepath) {
    auto task = std::make_unique<ImportTask>();
    task->owner = this;
    task->filepath = filepath;
    taskScheduler.AddTaskSetToPipe(task.get());
    tasks.push_back(std::move(task));
}

std::vector<ModelImporter::FinishedImport> ModelImporter::pollFinishedImports() {
    std::lock_guard<std::mutex> lock(finishedMutex);
    auto result = std::move(finished);
    finished.clear();
    return result;
}

void ModelImporter::ImportTask::ExecuteRange(enki::TaskSetPartition, uint32_t) {
    std::shared_ptr<Model> model;
    try {
        model = Model::createModelFromFile(owner->device, owner->textureManager, owner->materialManager, filepath);
    } catch (const std::exception& e) {
        // Runs on a worker thread - never let a malformed drop bring the app down.
        std::fprintf(stderr, "Model import failed for %s: %s\n", filepath.c_str(), e.what());
        return;
    }
    if (!model) return;  // missing file already logged by createModelFromFile

    std::lock_guard<std::mutex> lock(owner->finishedMutex);
    owner->finished.push_back({filepath, std::move(model)});
}
