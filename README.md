
#   📝 Overview

This project is a real-time global illumination demo built with **DirectX 12 / C++ / HLSL**, extending **Microsoft’s MiniEngine** framework.

It implements a hybrid **Surfel-Based Global Illumination (GI)** system combined with Horizon-Based Indirect Lighting (HBIL) — a screen-space method originally developed by Benoît “Patapom” Mayaux.

The approach focuses on _optimizing lighting coverage and efficiency by reducing redundant surfel spawning in complex regions_ while maintaining near-field lighting fidelity.
At the core of the system lies the Near-Far Surfel Sphere (NFSS) structure, which dynamically blends HBIL and surfel data to produce stable, high-quality indirect illumination in real time.

This project highlights my work in advanced graphics programming, ray tracing (DXR), and hybrid lighting techniques.

Here you can see the full overview of this [project](https://tedishopov.github.io/SurfelBasedGI.html).

For a full overview of my projects, please visit my [portfolio website](https://tedishopov.github.io/).

<img src="./Images/SurfelGIBanner.png" alt="Surfel GI Visualization" />

#   🛠️ Features

*   Hybrid Surfel + HBIL global illumination system

*   Near-Far Surfel Sphere (NFSS) structure for selective surfel instantiation

*   Confidence-based blending between HBIL and surfel irradiance

*   Screen-space horizon sampling and bent-cone estimation

*   Integration with MiniEngine’s deferred pipeline and DXR path

*   Debug visualization overlays for surfel coverage and hybrid blending zones

*   Real-time parameter control for density, thresholds, and blend weights

#   ⚙️ Tech Stack

*   Language: **C++**

*   Graphics API: **DirectX 12 (DXR)**

*   Shader Language: **HLSL**

*   Engine: **Microsoft MiniEngine (Extended)**

*   Tools: **PIX, RenderDoc, custom GPU profiling passes**

*   Platform: **Windows 10/11**


#   🙌 Credits & References

MiniEngine — base rendering framework by Microsoft, extended with custom passes and shader pipelines.

HBIL (Horizon-Based Indirect Lighting) by Benoît “Patapom” Mayaux — theory and original implementation available in the GodComplex repository - 
[GitHub link](https://github.com/Patapom/GodComplex/blob/master/Tests/TestHBIL/2018%20Mayaux%20-%20Horizon-Based%20Indirect%20Lighting%20(HBIL).pdf).

Research inspired by “PICA PICA” (2018) and “GIBS” (2021) real-time GI methods.