#version 410 core
// Grade pass: warm / cold tint + vignette + optional film grain, then linear -> sRGB (LinearTosRGB in r151).
// The scene texture is sampled NEAREST, so the low-res target upscales into crisp pixels.
in vec2 vUv;

uniform sampler2D uTex;
uniform float uVignette;
uniform float uWarm;
uniform float uGrain;
uniform float uTime;

out vec4 fragColor;

vec3 linearToSRGB(vec3 v) {
    vec3 hi = pow(max(v, vec3(0.0)), vec3(0.41666)) * 1.055 - vec3(0.055);
    vec3 lo = v * 12.92;
    return mix(hi, lo, vec3(lessThanEqual(v, vec3(0.0031308))));
}

void main() {
    vec4 c = texture(uTex, vUv);
    c.rgb *= vec3(1.0 + uWarm, 1.0, 1.0 - uWarm);
    vec2 q = vUv - 0.5;
    c.rgb *= max(1.0 - dot(q, q) * uVignette * 2.0, 0.0);
    if (uGrain > 0.0) {
        // hash noise that changes every frame
        float n = fract(sin(dot(gl_FragCoord.xy + fract(uTime) * 917.0, vec2(12.9898, 78.233))) * 43758.5453);
        c.rgb += (n - 0.5) * uGrain;
    }
    fragColor = vec4(linearToSRGB(c.rgb), 1.0);
}
