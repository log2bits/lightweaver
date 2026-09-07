# Lightweaver

A real-time, physically based **deferred renderer** built from scratch in C++ and Vulkan.

The goal is a real time renderer written and optimized by hand to learn the API end to end, and get familiar with GPU profiling.

## Planned Features

**Lighting & materials**
- [ ] PBR materials - metallic/roughness, normal mapping
- [ ] Point lights - clustered/froxel light culling *(later)*
- [ ] Directional sunlight
- [ ] Shadow mapping with PCF - cascaded shadow maps *(later)*
- [ ] Ambient occlusion (GTAO)
- [ ] Image-based lighting from the sky - diffuse + specular IBL
- [ ] Parallax occlusion mapping *(later)*
- [ ] Global illumination *(much later, this is a hard nut)*

**Geometry & culling**
- [ ] GPU-driven culling - frustum + backface + hi-Z occlusion

**Atmosphere & volumetrics**
- [ ] Physically based sky / atmospheric scattering
- [ ] Froxel-based volumetric fog

**Transparency & refraction**
- [ ] Transparent materials - order-independent transparency *(TBD)*
- [ ] Screen-space refraction for glass - UV displacement

**Post-processing**
- [ ] HDR pipeline + tonemapping (OpenDRT or similar)
- [ ] Bloom - progressive down/up mip chain
- [ ] Anti-aliasing (MSAA or TAA)

**Validation**
- [ ] Hardware ray tracing (BVH) for ground-truth comparison

## Built with

C++20 · Vulkan 1.3 (dynamic rendering, bindless) · GLSL shaders · CMake + Ninja · VMA · SDL3 · glm

*Named for the Lightweavers of the Cosmere, who weave light and sound into illusion.*
