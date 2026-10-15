#pragma once
// Procedural trees, built once per variant and drawn instanced.
//   Pine: tall bending trunk (bark texture), whorls of drooping branch cards (alpha-tested "branch" texture)
//         starting well above head height, a few dead twigs low on the trunk.
//   Dead: grey trunk with bare branches, no needles.
// Local space: base of the trunk at the origin, +Y up, height ~7–14 m.
#include <ter/ter.h>

struct TreeModel {
    ter::GeoPtr trunk;     // bark (trunk + twigs)
    ter::GeoPtr foliage;   // branch cards; null for a dead tree
    float baseRadius = 0.25f;
    bool dead = false;
};

TreeModel makePine(int seed);
TreeModel makeDeadTree(int seed);
