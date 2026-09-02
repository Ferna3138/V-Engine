struct PointLight{
    vec4 position;
    vec4 colour;
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
