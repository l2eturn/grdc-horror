#pragma once
// Object3D scene graph — a small subset of three.js (Group / Mesh / InstancedMesh / Points / Sprite / PointLight / SpotLight)
// so the world-building code from the web ports almost line for line.
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ter/scene/geometry.h"
#include "ter/scene/material.h"

namespace ter {

enum class ObjectKind { Group, Mesh, Points, Sprite, PointLight };

class Object3D;
using ObjPtr = std::shared_ptr<Object3D>;

class Object3D : public std::enable_shared_from_this<Object3D> {
public:
    virtual ~Object3D() = default;

    std::string name;
    ObjectKind kind = ObjectKind::Group;

    glm::vec3 position{0};
    // Euler angles, XYZ order (three.js default)
    glm::vec3 rotation{0};
    glm::vec3 scale{1};
    // when set (lookAt, setQuaternion) the quaternion replaces `rotation`
    bool useQuaternion = false;
    glm::quat quaternion{1, 0, 0, 0};

    bool visible = true;
    bool castShadow = false, receiveShadow = false;
    int renderOrder = 0;

    Object3D* parent = nullptr;
    std::vector<ObjPtr> children;
    glm::mat4 matrixWorld{1};

    // ---------- Mesh / Points / Sprite ----------
    GeoPtr geometry;
    // one material, or one per geometry group (material arrays)
    std::vector<MatPtr> materials;
    bool multiMaterial = false;
    MatPtr material() const { return materials.empty() ? nullptr : materials[0]; }

    // ---------- InstancedMesh ----------
    bool instanced = false;
    std::vector<glm::mat4> instances;
    bool instancesDirty = true;
    unsigned instanceVbo = 0;

    // ---------- PointLight ----------
    glm::vec3 lightColor{1};
    float intensity = 1, distance = 0, decay = 2;
    // spot light: half-angle of the cone in radians (0 = point light), soft edge 0..1; shines along local +Z
    float angle = 0, penumbra = 0;

    // called every frame before the object is drawn (three.js onBeforeRender)
    std::function<void(Object3D&)> onBeforeRender;

    // ---------- graph ----------
    Object3D& add(const ObjPtr& child);
    template <class... T>
    Object3D& add(const ObjPtr& a, const T&... rest) {
        add(a);
        return add(rest...);
    }
    void remove(Object3D* child);
    void removeFromParent();
    void traverse(const std::function<void(Object3D&)>& fn);

    glm::mat4 localMatrix() const;
    glm::quat localQuaternion() const;
    void updateMatrixWorld(const glm::mat4& parentWorld = glm::mat4(1));
    // updates this object's matrixWorld from its ancestors (like updateMatrixWorld(true) on the root)
    void updateWorldFromParents();
    glm::vec3 worldPosition() const { return glm::vec3(matrixWorld[3]); }
    glm::vec3 localToWorld(glm::vec3 p) const { return glm::vec3(matrixWorld * glm::vec4(p, 1)); }
    // yaw of the world rotation (Euler 'YXZ' .y) — used by the sprite billboard code
    float worldYaw() const;
    // Object3D.lookAt for non-camera objects: +Z points at the world-space target
    void lookAt(glm::vec3 target);
    void setQuaternion(glm::quat q) {
        quaternion = q;
        useQuaternion = true;
    }

    // shallow clone: shares geometry/materials, copies transform, clones children
    ObjPtr clone() const;
};

ObjPtr makeGroup(const std::string& name = "");
ObjPtr makeMesh(const GeoPtr& g, const MatPtr& m);
ObjPtr makeMesh(const GeoPtr& g, const std::vector<MatPtr>& mats);
ObjPtr makeInstanced(const GeoPtr& g, const std::vector<MatPtr>& mats, size_t count);
ObjPtr makePoints(const GeoPtr& g, const MatPtr& m);
ObjPtr makeSprite(const MatPtr& m);
ObjPtr makePointLight(glm::vec3 color, float intensity, float distance = 0, float decay = 2);
// a cone light (flashlight / street lamp) shining along its local +Z; aim it with lookAt(target)
ObjPtr makeSpotLight(glm::vec3 color, float intensity, float distance, float angle, float penumbra = 0.3f, float decay = 2);

// TRS helper: compose(position, rotationY, scale) as used for InstancedMesh matrices
glm::mat4 composeMatrix(glm::vec3 p, glm::quat q, glm::vec3 s);
glm::quat eulerToQuat(glm::vec3 xyz);

}  // namespace ter
