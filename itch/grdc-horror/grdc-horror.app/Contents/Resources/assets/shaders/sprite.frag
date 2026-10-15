#version 410 core
// grdc-horror override of terEngine's sprite shader, used for the drifting smoke puffs.
//  - lit sprites (spriteMaterial) take the ambient light and every lamp / the flashlight at their centre,
//    so smoke glows near a street lamp and when the flashlight points at it
//  - they fade out when the camera gets close (walking through smoke never shows a flat card)
//  - far away they sink into the dark fog, like everything else
// Unlit sprites (ter::basic material) skip the lighting.
in vec2 vUV;
in float vFogDepth;
in vec3 vCenter;

uniform int uType;
uniform vec3 uColor;
uniform float uOpacity;
uniform sampler2D uMap;
uniform int uHasMap;
uniform float uAlphaTest;
uniform int uFog;
uniform vec3 uFogColor;
uniform float uFogFar;   // visibility (m), see mesh.frag
uniform vec3 uHemiSky;
uniform vec3 uHemiGround;

#define MAX_LIGHTS 16
uniform int uNumLights;
uniform vec3 uLightPos[MAX_LIGHTS];
uniform vec3 uLightColor[MAX_LIGHTS];
uniform float uLightDist[MAX_LIGHTS];
uniform float uLightDecay[MAX_LIGHTS];
uniform vec3 uLightDir[MAX_LIGHTS];
uniform float uLightCosOuter[MAX_LIGHTS];
uniform float uLightCosInner[MAX_LIGHTS];

out vec4 fragColor;

const float NEAR_FADE_START = 1.0;   // fully gone at this distance from the camera ...
const float NEAR_FADE_END = 4.5;     // ... fully visible from here

float distanceAttenuation(float d, float cutoff, float decay) {
    if (cutoff > 0.0 && decay > 0.0) return pow(clamp(-d / cutoff + 1.0, 0.0, 1.0), decay);
    return 1.0;
}

void main() {
    vec4 c = vec4(uColor, uOpacity);
    if (uHasMap == 1) c *= texture(uMap, vUV);
    if (c.a < uAlphaTest) discard;
    vec3 rgb = c.rgb;
    if (uType != 0) {
        vec3 light = (uHemiSky + uHemiGround) * 0.5;
        for (int i = 0; i < uNumLights; ++i) {
            vec3 lv = uLightPos[i] - vCenter;
            float d = length(lv);
            float cone = smoothstep(uLightCosOuter[i], uLightCosInner[i], dot(-lv / max(d, 1e-4), uLightDir[i]));
            // spot lights (the flashlight) only tint the puff a little, or it flashes up as a bright disc
            float k = uLightCosOuter[i] < -1.5 ? 1.0 : 0.25;
            light += uLightColor[i] * distanceAttenuation(d, uLightDist[i], uLightDecay[i]) * cone * k;
        }
        rgb *= light;
    }
    float alpha = c.a * smoothstep(NEAR_FADE_START, NEAR_FADE_END, vFogDepth);
    if (uFog == 1) {
        float f = 1.0 - exp(-3.0 * vFogDepth / max(uFogFar, 0.5));
        rgb = mix(rgb, uFogColor * 0.4, f);
        alpha *= 1.0 - smoothstep(uFogFar * 0.9, uFogFar * 1.4, vFogDepth);
    }
    fragColor = vec4(rgb, alpha);
}
