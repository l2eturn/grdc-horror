#version 410 core
// grdc-horror override of terEngine's mesh shader (loaded instead of the built-in one because main.cpp
// calls ter::addShaderPath(assets/shaders) before creating the App).
//
// Same lighting as the engine (hemisphere + sun + point/spot lights), plus two changes:
//  1. VOLUMETRIC FOG instead of the engine's linear distance fog. Every pixel walks along the view ray,
//     adds up fog that is thick near the ground, thin higher up, rolling like smoke (warped 3D noise) and
//     drifting with the wind, lit by the scene's lights (lamps glow, the flashlight beam shows), and fading
//     into darkness far away.
//     map.json "atmosphere": fogColor = colour of unlit fog, visibility = meters until you can't see
//     through ground fog, fogHeight = how fast it thins with height (meters per halving-ish).
//     Finer tuning: the FOG_* constants below.
//  2. Foliage lighting: double-sided alpha-tested surfaces (leaves, grass) keep their normal on both
//     sides instead of flipping it, so branch cards shade like a volume, not like paper.
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec3 vColor;
in float vFogDepth;
in vec4 vShadowCoord;
in float vBlend;

// 0 = basic, 1 = lambert, 2 = standard
uniform int uType;
uniform vec3 uColor;
uniform float uOpacity;
uniform sampler2D uMap;
uniform int uHasMap;
uniform sampler2D uMap2;
uniform int uHasMap2;
uniform float uAlphaTest;
uniform int uVertexColors;
uniform vec3 uEmissive;
uniform sampler2D uEmissiveMap;
uniform int uHasEmissiveMap;
uniform float uRoughness;
uniform float uMetalness;
// 0 = front, 1 = back, 2 = double
uniform int uSide;

uniform vec3 uHemiSky;
uniform vec3 uHemiGround;
uniform vec3 uSunColor;
uniform vec3 uSunDir;
uniform int uReceiveShadow;
uniform sampler2DShadow uShadowMap;
uniform float uShadowBias;
uniform float uShadowTexel;

#define MAX_LIGHTS 16
uniform int uNumLights;
uniform vec3 uLightPos[MAX_LIGHTS];
uniform vec3 uLightColor[MAX_LIGHTS];
uniform float uLightDist[MAX_LIGHTS];
uniform float uLightDecay[MAX_LIGHTS];
uniform vec3 uLightDir[MAX_LIGHTS];
uniform float uLightCosOuter[MAX_LIGHTS];
uniform float uLightCosInner[MAX_LIGHTS];

uniform vec3 uCamPos;
uniform float uTime;
uniform int uFog;
uniform vec3 uFogColor;
// reused by this shader: uFogNear = fog height scale (m), uFogFar = visibility at ground level (m)
uniform float uFogNear;
uniform float uFogFar;
uniform int uClip;
uniform vec4 uClipPlane;

out vec4 fragColor;

const float PI = 3.141592653589793;

// ---------------------------------------------------------------- fog tuning
const int   FOG_STEPS = 20;                       // samples along each view ray (more = smoother, slower)
const float FOG_MAX_DIST = 70.0;                  // fog is gathered up to this distance (sky and mountains lie beyond)
const float FOG_GROUND = 0.0;                     // height where the fog is thickest
const float FOG_CLUMP_SIZE = 7.0;                 // meters; size of the smoke banks
const float FOG_PATCHINESS = 1.0;                 // 0 = even fog, 1 = clear gaps between thick banks
const vec3  FOG_WIND = vec3(0.45, 0.0, 0.18);     // m/s drift of the banks
const float FOG_SWIRL = 0.05;                     // how fast the smoke rolls and changes shape
const float FOG_LIGHT_GLOW = 1.1;                 // how strongly lamps and the flashlight light up the fog
const float FOG_HALO = 0.6;                       // extra soft glow right around point lights
const float FOG_BEAM = 0.05;                      // how visible spot-light beams (the flashlight) are in the fog
const float FOG_DARK_NEAR = 6.0;                  // fog is lit like fogColor up to here ...
const float FOG_DARK_FAR = 35.0;                  // ... and fades into darkness by here (the world ends in black, like DOOM)
const float FOG_DARK_LEVEL = 0.25;                // brightness of the far fog compared to the near fog

// getDistanceAttenuation() for legacy lights
float distanceAttenuation(float d, float cutoff, float decay) {
    if (cutoff > 0.0 && decay > 0.0) return pow(clamp(-d / cutoff + 1.0, 0.0, 1.0), decay);
    return 1.0;
}

float shadowFactor() {
    vec3 c = vShadowCoord.xyz / vShadowCoord.w;
    if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0 || c.z > 1.0) return 1.0;
    c.z += uShadowBias;
    float s = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) s += texture(uShadowMap, vec3(c.xy + vec2(x, y) * uShadowTexel, c.z));
    return s / 9.0;
}

vec3 brdfGGX(vec3 l, vec3 v, vec3 n, vec3 f0, float roughness) {
    float alpha = roughness * roughness;
    vec3 h = normalize(l + v);
    float dotNL = clamp(dot(n, l), 0.0, 1.0);
    float dotNV = clamp(dot(n, v), 0.0, 1.0);
    float dotNH = clamp(dot(n, h), 0.0, 1.0);
    float dotVH = clamp(dot(v, h), 0.0, 1.0);
    float fresnel = exp2((-5.55473 * dotVH - 6.98316) * dotVH);
    vec3 F = f0 * (1.0 - fresnel) + fresnel;
    float a2 = alpha * alpha;
    float gv = dotNL * sqrt(a2 + (1.0 - a2) * dotNV * dotNV);
    float gl = dotNV * sqrt(a2 + (1.0 - a2) * dotNL * dotNL);
    float V = 0.5 / max(gv + gl, 1e-6);
    float denom = dotNH * dotNH * (a2 - 1.0) + 1.0;
    float D = (1.0 / PI) * a2 / (denom * denom);
    return F * (V * D);
}

// ---------------------------------------------------------------- volumetric fog

float hash3(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.71, 0.113, 0.419));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float valueNoise3(vec3 x) {
    vec3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash3(i), hash3(i + vec3(1, 0, 0)), f.x), mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), f.x), mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

// fog particles per meter at point p: rolling smoke banks (domain-warped noise) that thin out with height
float fogDensity(vec3 p) {
    float base = 3.0 / max(uFogFar, 0.5);                                    // ~95% hidden after `visibility` m
    float height = exp(-max(p.y - FOG_GROUND, 0.0) / max(uFogNear, 0.1));
    vec3 q = (p - FOG_WIND * uTime) / FOG_CLUMP_SIZE;
    // warp the lookup by another slow noise, so the banks curl and change shape instead of just sliding
    float t = uTime * FOG_SWIRL;
    vec3 warp = vec3(valueNoise3(q * 0.6 + vec3(0.0, t, 4.1)), valueNoise3(q * 0.6 + vec3(5.2, t, 1.3)), valueNoise3(q * 0.6 + vec3(2.9, t, 7.7)));
    q += (warp - 0.5) * 1.6;
    float n = valueNoise3(q) * 0.65 + valueNoise3(q * 2.1 + 7.7) * 0.35;      // 0..1, smooth (no fine grain)
    return base * height * mix(1.0, smoothstep(0.3, 0.72, n) * 2.2, FOG_PATCHINESS);
}

// light reaching the fog at p from the scene's point and spot lights
vec3 fogLight(vec3 p) {
    vec3 sum = vec3(0.0);
    for (int i = 0; i < uNumLights; ++i) {
        vec3 lv = uLightPos[i] - p;
        float d = length(lv);
        float cone = smoothstep(uLightCosOuter[i], uLightCosInner[i], dot(-lv / max(d, 1e-4), uLightDir[i]));
        float atten = distanceAttenuation(d, uLightDist[i], uLightDecay[i]);
        cone *= cone;   // softer beam edge in the fog
        if (uLightCosOuter[i] < -1.5) {
            // point light: lit haze around it plus a soft halo close in
            sum += uLightColor[i] * (atten * 0.2 + FOG_HALO / (1.0 + d * d * 1.5));
        } else {
            // spot light (the flashlight): a faint beam; the first meter in front of the lens stays dark,
            // otherwise the fog right at the camera glows and hides everything
            sum += uLightColor[i] * atten * cone * FOG_BEAM * smoothstep(0.4, 3.0, d);
        }
    }
    return sum;
}

vec3 applyFog(vec3 color) {
    vec3 ray = vWorld - uCamPos;
    float dist = length(ray);
    vec3 dir = ray / max(dist, 1e-4);
    float len = min(dist, FOG_MAX_DIST);
    float transmit = 1.0;
    vec3 gathered = vec3(0.0);
    float prev = 0.0;
    // fixed sample positions (no per-frame random jitter: that made the fog fizz like TV static)
    for (int i = 1; i <= FOG_STEPS; ++i) {
        // samples bunch up near the camera, where detail (and the flashlight beam) matters most
        float s = (float(i) - 0.5) / float(FOG_STEPS);
        float t = len * s * s;
        float dt = t - prev;
        prev = t;
        vec3 p = uCamPos + dir * t;
        float a = 1.0 - exp(-fogDensity(p) * dt);
        float dark = mix(1.0, FOG_DARK_LEVEL, smoothstep(FOG_DARK_NEAR, FOG_DARK_FAR, t));
        vec3 lit = uFogColor * dark + FOG_LIGHT_GLOW * fogLight(p);
        gathered += transmit * a * lit;
        transmit *= 1.0 - a;
    }
    // the rest of the ray (between the last sample and the surface)
    float a = 1.0 - exp(-fogDensity(uCamPos + dir * len) * (len - prev));
    gathered += transmit * a * uFogColor * mix(1.0, FOG_DARK_LEVEL, smoothstep(FOG_DARK_NEAR, FOG_DARK_FAR, len));
    transmit *= 1.0 - a;
    return color * transmit + gathered;
}

void main() {
    if (uClip == 1 && dot(uClipPlane.xyz, vWorld) + uClipPlane.w < 0.0) discard;

    vec4 diffuse = vec4(uColor, uOpacity);
    if (uHasMap == 1) {
        vec4 t = texture(uMap, vUV);
        if (uHasMap2 == 1) t = mix(t, texture(uMap2, vUV), clamp(vBlend, 0.0, 1.0));
        diffuse *= t;
    }
    if (uVertexColors == 1) diffuse.rgb *= vColor;
    if (diffuse.a < uAlphaTest) discard;

    vec3 outgoing;
    if (uType == 0) {
        outgoing = diffuse.rgb;
    } else {
        vec3 n = normalize(vNormal);
        bool foliage = uSide == 2 && uAlphaTest > 0.0;
        if (uSide == 1) n = -n;
        if (uSide == 2 && !gl_FrontFacing && !foliage) n = -n;
        vec3 v = normalize(uCamPos - vWorld);
        vec3 albedo = uType == 2 ? diffuse.rgb * (1.0 - uMetalness) : diffuse.rgb;
        vec3 f0 = mix(vec3(0.04), diffuse.rgb, uMetalness);
        float rough = clamp(uRoughness, 0.0525, 1.0);

        vec3 irradiance = mix(uHemiGround, uHemiSky, 0.5 * n.y + 0.5);
        vec3 direct = vec3(0.0);
        vec3 spec = vec3(0.0);

        float sh = uReceiveShadow == 1 ? shadowFactor() : 1.0;
        float dotNL = max(dot(n, uSunDir), 0.0);
        vec3 sunIrr = uSunColor * dotNL * sh;
        direct += sunIrr;
        if (uType == 2) spec += sunIrr * brdfGGX(uSunDir, v, n, f0, rough);

        for (int i = 0; i < uNumLights; ++i) {
            vec3 lv = uLightPos[i] - vWorld;
            float d = length(lv);
            vec3 l = lv / max(d, 1e-5);
            vec3 c = uLightColor[i] * distanceAttenuation(d, uLightDist[i], uLightDecay[i]);
            c *= smoothstep(uLightCosOuter[i], uLightCosInner[i], dot(-l, uLightDir[i]));
            // foliage lets light through: wrap the lighting around instead of a hard dark side
            float ndl = foliage ? dot(n, l) * 0.5 + 0.5 : max(dot(n, l), 0.0);
            vec3 irr = c * ndl;
            direct += irr;
            if (uType == 2) spec += irr * brdfGGX(l, v, n, f0, rough);
        }

        vec3 emissive = uEmissive;
        if (uHasEmissiveMap == 1) emissive *= texture(uEmissiveMap, vUV).rgb;
        outgoing = albedo * (direct + irradiance) + spec + emissive;
    }

    if (uFog == 1) outgoing = applyFog(outgoing);
    fragColor = vec4(outgoing, diffuse.a);
}
