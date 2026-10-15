#pragma once
// Texture kit: small PS2-style textures (64–256 px, bilinear) painted in code, so the game needs no image files.
// Ask for one by name; it is generated once and shared.
//
//   asphalt  8 m of 2-lane road: wheel tracks, a patch, cracks, worn lines (u = along the road, v = across)
//   grass    dark mountain grass          dirt     packed earth with pebbles
//   rock     grey stone with lichen       cliff    vertical rock face
//   bark     pine trunk (128x256)         needles  pine foliage    branch  RGBA pine branch for foliage cards
//   concrete stained grey concrete        metal    rusty guardrail
//   grasscard  RGBA grass blades for alpha cards     mist  soft tileable fog wisps (RGBA)
//   glow     soft round falloff (halos)    disc     round with a soft rim (moon)
//   smoke, smoke2  soft puffs of smoke (RGBA) for drifting smoke sprites
//   grip     knurled rubber (flashlight)   corrugated  galvanised roof sheet (ridges across u)
//   wood     weathered planks              paint    chipped white paint on plaster (tint it)
//   steel    galvanised poles and legs     skin, cloth  the player's hand and sleeve
//   puddle, litter, stain, skid  RGBA decals for the road surface
#include <memory>
#include <string>
#include <ter/ter.h>

// The texture with this name (nullptr and a warning for an unknown name)
std::shared_ptr<ter::Texture> texture(const std::string& name);
