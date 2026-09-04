#pragma once

#include "Camera.hpp"          // CamParameters
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

struct CameraExposure {
    float     scale = 1.f;              // linear multiplier for the tonemap pass
    glm::vec3 whiteBalanceGain{1.f};    // per-channel gain, 6500 K -> (1,1,1)
    float autoIso = 100.f;   // solved to hold EV100 constant; for display + future grain
};

// Kelvin -> linear-ish RGB (Tanner Helland approximation), valid ~1000-40000 K.
inline glm::vec3 kelvinToRGB(float kelvin) {
    float t = kelvin / 100.f;
    float r, g, b;
    if (t <= 66.f) {
        r = 255.f;
        g = 99.4708025861f * std::log(t) - 161.1195681661f;
        b = (t <= 19.f) ? 0.f : 138.5177312231f * std::log(t - 10.f) - 305.0447927307f;
    } else {
        r = 329.698727446f * std::pow(t - 60.f, -0.1332047592f);
        g = 288.1221695283f * std::pow(t - 60.f, -0.0755148492f);
        b = 255.f;
    }
    return glm::clamp(glm::vec3(r, g, b) / 255.f, glm::vec3(0.f), glm::vec3(1.f));
}


inline CameraExposure computeExposure(const CamParameters& p) {
    CameraExposure out;

    // The scene is locked to a fixed exposure target. Aperture and shutter angle
    // are creative controls (DoF / motion blur) and must not change brightness,
    // so ISO is solved to hold EV100 constant. Only EV Comp shifts exposure.
    constexpr float kTargetEV100      = 10.0f;   // the "correct" exposure the scene is authored for
    // Lights now carry luminous power (lm) and the BRDF is normalised, so the
    // shader output is already ~photometric. This is a small residual gain to
    // land middle-grey at the target EV — tune once for your content.
    constexpr float kSceneCalibration = 180.0f;

    out.scale = kSceneCalibration / (1.2f * std::exp2(kTargetEV100));
    out.scale *= std::exp2(p.exposure);           // EV Comp — the only brightness control
    out.scale = std::clamp(out.scale, 1e-4f, 1e4f);

    // Auto-ISO: what the sensor gain would have to be. Not fed into out.scale.
    float fps = (p.fps > 0.f) ? p.fps : 24.f;
    float t   = (p.shutter_angle / 360.f) / fps;
    if (t > 0.f && p.aperture > 0.f)
        out.autoIso = std::clamp(
            100.f * (p.aperture * p.aperture / t) / std::exp2(kTargetEV100), 50.f, 128000.f);

    glm::vec3 ref = kelvinToRGB(6500.f);
    glm::vec3 cur = kelvinToRGB(static_cast<float>(p.white_balance));
    out.whiteBalanceGain = ref / glm::max(cur, glm::vec3(1e-4f));
    return out;
}