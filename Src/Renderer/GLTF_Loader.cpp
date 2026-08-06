#define TINYGLTF3_IMPLEMENTATION
#include <tiny_gltf_v3.h>
#include "Renderer/GLTF_Loader.hpp"
#include <assert.h>
#include <iostream>

void GLTF_Smoke::GLTF_SmokeTest(const std::string& filepath) {
    std::cout << "[SMOKE TEST] Starting test for: " << filepath << std::endl;
    
    tg3_parse_options options;
    tg3_parse_options_init(&options);

    options.images_as_is            = 1;  // Don't decode images
    options.preserve_image_channels = 1;  // Keep original channels
    options.store_original_json     = 1;  // Store raw JSON strings
    options.skip_extras_values      = 1;  // Skip extras & extension trees
    options.borrow_input_buffers    = 1;  // Point directly into caller data
    options.parse_float32           = 1;  // Fast float32 parsing
    options.validate_indices        = 1;  // Validate array index bounds
    options.max_external_file_size  = 0;  // No external file size limit

    options.fs       = {};
    options.uri      = {};
    options.image    = {};
    options.stream   = nullptr;
    options.progress = nullptr;

    tg3_error_stack errors;
    tg3_error_stack_init(&errors);

    tg3_model model;
    tg3_error_code status = tg3_parse_file(&model, &errors, filepath.c_str(), (uint32_t)filepath.size(), &options);

    // Stream out any parsing errors
    for (uint32_t i = 0; i < errors.count; i++) {
        std::cerr << "GLTF Error [" << i << "]: " << errors.entries[i].message << std::endl;
    }

    assert(status == TG3_OK && "GLTF parsing failed!");

    // Use '<<' stream chaining instead of '+'
    std::cout << "Meshes Count: " << model.meshes_count << std::endl;
    std::cout << "Texture Count: " << model.textures_count << std::endl;
    std::cout << "Scenes Count: " << model.scenes_count << std::endl;

    tg3_model_free(&model);
    tg3_error_stack_free(&errors);
}