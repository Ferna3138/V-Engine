struct PointLight{
    vec4 position;   // xyz = world pos, w = influence range
    vec4 colour;     // rgb = colour, w = luminous power (lumens)
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
    mat4 invProj;      // add this — matches C++ inverseProj, even if unused for now
    vec4 ambientLightColor;
    PointLight pointLights[10];
    int numLights;
} ubo;
