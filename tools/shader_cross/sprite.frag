#version 310 es
precision highp float;

// Same Sprite block as the hand-written HLSL test, written in GLSL ES.
// Translated with:
//   glslang -g -V -S frag --target-env vulkan1.1 sprite.frag -o sprite.spv
//   spirv-cross sprite.spv --hlsl --shader-model 50 --entry main
// std140 puts the vec2 on an 8-byte boundary, so scale lands at byte 24.
layout(std140, binding = 7) uniform Sprite {
    vec4 tint;
    float extra;
    vec2 scale;
    mat4 world;
    float weights[2];
};

layout(location = 0) out vec4 frag_color;

void main()
{
    float used = scale.x + scale.y + world[0][0] + world[1][1] + weights[0] + weights[1];
    frag_color = tint * extra + vec4(used, 0.0, 0.0, 0.0);
}
