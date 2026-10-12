# Custom shaders

[← Back to README](../README.md) · Headers: `ter/gfx/shader.h`, `ter/scene/material.h`

When the built-in materials aren't enough (a wobbling ghost, a dissolving wall, a static-noise TV screen), write your own GLSL and use it as a material.

---

## A custom material, step by step

**1. Tell the engine where your shaders live** (once, at startup):

```cpp
ter::addShaderPath(ter::assetPath("shaders"));
```

**2. Write `assets/shaders/ghost.vert` and `assets/shaders/ghost.frag`:**

```glsl
// ghost.vert
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uModel, uView, uProj;
uniform float uTime;
out vec2 vUV;

void main() {
    vec3 p = aPos;
    p.x += sin(p.y * 6.0 + uTime * 3.0) * 0.05;   // wobble sideways
    vUV = aUV;
    gl_Position = uProj * uView * uModel * vec4(p, 1.0);
}
```

```glsl
// ghost.frag
#version 410 core
in vec2 vUV;
uniform float uFade;
out vec4 fragColor;

void main() {
    float glow = 1.0 - length(vUV - 0.5) * 2.0;
    fragColor = vec4(vec3(0.6, 0.8, 1.0) * glow, glow * uFade);   // linear color
}
```

**3. Make the material:**

```cpp
float fade = 0.7f;

auto mat = std::make_shared<ter::Material>();
mat->type = ter::MaterialType::Custom;
mat->shader = ter::sharedShader("ghost");     // loads ghost.vert + ghost.frag once, then caches
mat->transparent = true;
mat->depthWrite = false;
mat->uniforms = [&](const ter::Shader& s) {   // called every time the material is drawn
    s.set("uFade", fade);
};

auto ghost = ter::makeMesh(ter::geo::sphere(0.5f), mat);
scene->add(ghost);
```

---

## What the engine gives your shader

**Vertex attributes**

| Location | Name | Type |
|---|---|---|
| 0 | `aPos` | `vec3` |
| 1 | `aNormal` | `vec3` |
| 2 | `aUV` | `vec2` |
| 3 | `aColor` | `vec3` (vertex colors) |
| 4 | `aInstance` | `mat4` (instanced meshes only) |

**Uniforms set automatically**

| Uniform | Meaning |
|---|---|
| `uModel`, `uView`, `uProj`, `uNormalMatrix` | Transforms |
| `uInstanced` | Whether `aInstance` is in use |
| `uCamPos` | Camera position in world space |
| `uTime` | Seconds since start |
| `uResolution` | Render target size in pixels |
| `uMap`, `uHasMap` | The material's `map` texture |
| `uOpacity`, `uAlphaTest` | From the material |
| `uMapOffset`, `uMapRepeat` | From the material |

Set your own uniforms in `mat->uniforms` with `s.set(name, value)`. Value types: `float`, `int`, `vec2`, `vec3`, `vec4`, `mat3`, `mat4`, plus `setArray` for arrays.

**Rules**

- Use `#version 410 core` (OpenGL 4.1 is the highest macOS supports).
- Output **linear** color. The post-processing pass converts to sRGB.
- Custom shaders don't automatically get scene lighting or fog. Add them yourself if you need them.

---

## Hot reload

Bind a key to reload every shader from disk while the game runs. This is the fastest way to tweak effects:

```cpp
if (in.pressed(ter::Key::R)) ter::reloadSharedShaders();
```

Compile errors are printed to the terminal, and the old shader stays in use.

---

## Overriding built-in shaders

The built-in shaders (`mesh`, `depth`, `sprite`, `post`, `tilt`, `overlay`) are embedded in the library; their sources are in `shaders/`. A file with the same name in a folder added with `addShaderPath` replaces the built-in one. For example, copy `shaders/post.frag` into your `assets/shaders/` and edit it to change the whole game's color grading.

## Shaders from strings

```cpp
ter::Shader s;
s.loadSource(vertexSourceString, fragmentSourceString, "my-effect");
mat->shader = &s;   // s must outlive the material
```
