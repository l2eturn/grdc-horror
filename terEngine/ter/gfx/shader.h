#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "ter/gfx/gl.h"

namespace ter {

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Load <name>.vert + <name>.frag: first from the folders added with addShaderPath(), then from the
    // shaders built into the engine. `defines` is pasted after the #version line (e.g. "#define DEPTH_ONLY\n").
    // Returns false if compile/link fails (the log is printed).
    bool load(const std::string& name, const std::string& defines = "");
    // Compile from source strings (`label` only appears in error messages)
    bool loadSource(const std::string& vert, const std::string& frag, const std::string& label = "inline");
    // Reload from disk with the same name/defines (R key)
    bool reload() { return load(name_, defines_); }
    void use() const { glUseProgram(prog_); }
    GLuint id() const { return prog_; }

    void set(const char* n, const glm::mat4& m) const;
    void set(const char* n, const glm::mat3& m) const;
    void set(const char* n, const glm::vec4& v) const;
    void set(const char* n, const glm::vec3& v) const;
    void set(const char* n, const glm::vec2& v) const;
    void set(const char* n, float f) const;
    void set(const char* n, int i) const;
    void setArray(const char* n, const glm::vec3* v, int count) const;
    void setArray(const char* n, const float* v, int count) const;

private:
    bool link(const std::string& vsrc, const std::string& fsrc, const std::string& vlabel, const std::string& flabel);
    GLint loc(const char* n) const;
    GLuint prog_ = 0;
    std::string name_, defines_;
    mutable std::unordered_map<std::string, GLint> locs_;
};

// Shared programs for ShaderMaterials (MaterialType::Custom), loaded on first use and cached by name.
Shader* sharedShader(const std::string& name);
// Reload every cached shared program (R key)
void reloadSharedShaders();
// Add a folder searched (before the built-in shaders) for <name>.vert / <name>.frag.
// A file named like a built-in one (mesh, post, ...) overrides it.
void addShaderPath(const std::string& dir);

}  // namespace ter
