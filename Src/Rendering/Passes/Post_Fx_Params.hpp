#pragma once

#include <glm/glm.hpp>

// CPU-side mirrors of the push-constant blocks used by the built-in fullscreen
// post-processing passes. An app fills one of these per frame and hands it to the
// matching FullscreenPass via render(cb, &params, sizeof(params)); the layout here
// must stay byte-identical to the `layout(push_constant)` block in the paired
// shader. static_asserts below guard against silent drift.

// Src/Rendering/Shaders/dof_downsample.frag
struct DofDownsampleParams {
    glm::vec4 dof{0.f};    // x = focalLength(mm), y = fNumber, z = focusDistance(m), w = sensorHeight(mm)
    glm::vec4 extra{0.f};  // x = near, y = maxCoC(px), z = far, w = unused   (maxCoC 0 disables DoF)
};

// Src/Rendering/Shaders/dof_blur.frag
struct DofBlurParams {
    glm::vec4 params{0.f}; // x = apertureBlades, y = bladeRotation(rad), z = sampleCount, w = unused
};

// Src/Rendering/Shaders/tonemap.frag  (final composite + exposure + tone curve)
struct TonemapParams {
    glm::vec4 exposure{1.f}; // rgb = white-balance gain, a = exposure scale
};

// Src/Rendering/Shaders/motion_blur.frag
struct MotionBlurParams {
    glm::mat4 reprojection{1.f}; // prevViewProj * inverse(currViewProj)
    glm::vec4 params{0.f};       // x = shutterTime/frameTime, y = maxBlur(px), z = sampleCount, w = enabled
};

static_assert(sizeof(DofDownsampleParams) == 32, "push-constant layout drift");
static_assert(sizeof(DofBlurParams)       == 16, "push-constant layout drift");
static_assert(sizeof(TonemapParams)       == 16, "push-constant layout drift");
static_assert(sizeof(MotionBlurParams)    == 80, "push-constant layout drift");
