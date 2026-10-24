# Spec Delta

## Purpose

定义 Physara 轻量工程基线的构建、依赖追溯、模型与场景格式边界及完成证据，使后续功能能够在明确、可重建的起点上推进。

## ADDED Requirements

### Requirement: Rebuild the current baseline
The project SHALL build its implemented baseline with the documented Windows x64 MSVC toolchain in Debug and Release. Build products SHALL stay in the binary tree. The build SHALL NOT require old executables, old shader caches, a compiled absolute asset root or unimplemented business libraries.

#### Scenario: Clean supported build
- **WHEN** a maintainer configures a new binary directory and builds Debug and Release
- **THEN** the implemented source targets build from checked-in inputs without downloading dependencies

### Requirement: Trace curated dependencies
Each selected dependency SHALL have a fixed version or commit, upstream source, license and retained scope recorded in ThirdParty/README.md. Compiled dependencies SHALL build from retained sources. NVRHI SHALL remain a source reference outside the build.

#### Scenario: Offline dependency use
- **WHEN** the retained sources are present and a maintainer configures without network access
- **THEN** no submodule initialization, dependency download or manually prepared library is required

#### Scenario: Reference-only source
- **WHEN** CMake configures the project
- **THEN** NVRHI is absent from the targets, include paths and link dependencies

### Requirement: Separate model assets from editor scenes
The architecture SHALL define glTF/GLB as the model asset format and JSON as the editor scene format. It SHALL define model references, entity identity, hierarchy, instance state, ownership, load/save validation and failure behavior without duplicating model geometry or serializing GPU state. Current resource provenance and unsupported behavior SHALL be described honestly.

#### Scenario: Review persistence design
- **WHEN** a maintainer reads the Editor, Asset and Scene design
- **THEN** model import and editor scene persistence have consistent responsibilities, file locations and formats

### Requirement: Keep initialization lightweight
The project SHALL organize the current build with root and module CMakeLists.txt files and concise Chinese explanations. Initialization SHALL NOT create CI, standalone Tools programs, development-tool installation targets, repository formatting gates or a predefined test framework. Work-package completion SHALL reflect current executed checks and SHALL NOT imply unimplemented runtime or GPU success.

#### Scenario: Build without auxiliary tooling
- **WHEN** a maintainer builds the C++ project
- **THEN** the build does not install or invoke Node.js, npm, OpenSpec, a formatter or a test framework

#### Scenario: Report available evidence
- **WHEN** the baseline work is reported complete
- **THEN** the report identifies the actual build and document checks and distinguishes them from unimplemented product behavior
