<p align="center">
  <a href="#overview">
    <img src="Assets/Editor/icon_physara.png" width="220" alt="Physara renderer logo">
  </a>
</p>

<h1 align="center">Physara</h1>

<p align="center">
  <strong>A physically based renderer with a lightweight scene editor.</strong>
  <br>
</p>

<p align="center">
  <a href="Docs/Build.md"><img src="https://img.shields.io/badge/C%2B%2B-20-368D9B?style=flat&amp;labelColor=263844" alt="C++20"></a>
  <a href="RHI/README.md"><img src="https://img.shields.io/badge/OpenGL-4.6-76AFC4?style=flat&amp;labelColor=263844" alt="OpenGL 4.6"></a>
  <a href="Docs/Build.md"><img src="https://img.shields.io/badge/Windows-x64-667787?style=flat&amp;labelColor=263844" alt="Windows x64"></a>
  <a href="Docs/Developments/Developments.md#current-stage"><img src="https://img.shields.io/badge/Status-In%20development-D98F94?style=flat&amp;labelColor=263844" alt="In development"> </a>
</p>

<br>

## Overview

Physara is a **C++20 rendering project** for static scenes, with a lightweight editor for working with geometry, materials, lights, and cameras.

The goal is to create **Beautiful Scenery** through rendering algorithms that remain clear enough to study, refine, and maintain. The current implementation combines **physical Forward rendering**, an **OpenGL 4.6 backend**, and an integrated scene-editing workflow.

> **In active development.** The [development plan](Docs/Developments/Developments.md#current-stage) tracks completed capabilities and upcoming work.

## Design references

Physara draws on [Filament](Docs/Reference/Filament_Rendering_Pipeline_Analysis.md) for rendering and display design, and [NVRHI](Docs/Reference/NVRHI_RHI_Vulkan_Analysis.md) for rendering interfaces and resource management.

The linked notes document the ideas studied and the decisions made for Physara. Dependency versions and notices are collected in the [third-party guide](ThirdParty/README.md).
