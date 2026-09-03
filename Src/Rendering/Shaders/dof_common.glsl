// signed CoC in full-res pixels; negative = foreground
float cocPixels(float z, vec4 dof, vec2 extra, float screenH) {
    float f = dof.x, N = dof.y, focus = dof.z * 1000.0, zMM = z * 1000.0;
    float cocMM = f * f * (zMM - focus) / (zMM * (focus - f) * N);
    float maxCoC = extra.y;
    return clamp(cocMM * screenH / dof.w, -maxCoC, maxCoC);
}

float linearDepth(float d, float n, float f) { return (n * f) / (f - d * (f - n)); }