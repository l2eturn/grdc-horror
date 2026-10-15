#pragma once
// Small helpers for building props out of primitives (used by Props, Trees, Environment, Phone).
#include <string>
#include <ter/ter.h>

// A box stretched from a to b with the given cross-section (planks, wires, frames, twigs)
ter::GeoPtr beamGeo(glm::vec3 a, glm::vec3 b, float width, float height);

// A round tube from a to b, radius ra at a and rb at b (poles, pipes, arms)
ter::GeoPtr tubeGeo(glm::vec3 a, glm::vec3 b, float ra, float rb, int sides = 8);

// Lambert material using a texture from the kit (Textures.h), tinted, repeated `repeat` times
ter::MatPtr kitMat(const std::string& textureName, glm::vec3 tint = glm::vec3(1), glm::vec2 repeat = glm::vec2(1));

// A box whose texture keeps the same size on every face: one repeat every `tile` meters
ter::ObjPtr texturedBox(glm::vec3 size, const std::string& textureName, float tile, glm::vec3 tint = glm::vec3(1));

// A mesh placed at a position (and Euler rotation in radians)
ter::ObjPtr meshAt(const ter::GeoPtr& geo, const ter::MatPtr& mat, glm::vec3 pos, glm::vec3 rot = glm::vec3(0));
