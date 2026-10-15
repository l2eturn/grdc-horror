#version 410 core
// grdc-horror override of terEngine's sprite shader: same camera-facing quad, plus the sprite's world
// centre for lighting (see sprite.frag).
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec2 uMapOffset;
uniform vec2 uMapRepeat;

out vec2 vUV;
out float vFogDepth;
out vec3 vCenter;

void main() {
    vec4 mv = uView * uModel * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 scale = vec2(length(uModel[0].xyz), length(uModel[1].xyz));
    mv.xy += aPos.xy * scale;
    vUV = aUV * uMapRepeat + uMapOffset;
    vFogDepth = -mv.z;
    vCenter = uModel[3].xyz;
    gl_Position = uProj * mv;
}
