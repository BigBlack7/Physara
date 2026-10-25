# Spec Delta

## Purpose

Define the source-level shape of Physara's renamed public rendering contract, its independently usable CPU behavior and its traceability to the fixed upstream snapshot. Downstream consumers can prepare for later backends without requiring native SDKs or mistaking declarations for implemented graphics capabilities.

## ADDED Requirements

### Requirement: Consume the complete renamed public contract
The public contract SHALL expose the complete declaration surface of the documented public-header inputs from the fixed source through the Physara naming map. Resource, binding, pipeline, command, query, memory and extension declarations SHALL remain available regardless of current backend support. Changes to that surface SHALL be limited to documented naming, integration and explicitly enumerated correctness adaptations.

#### Scenario: Compile an ordinary graphics consumer
- **WHEN** a C++20 consumer uses resource descriptors, separate texture and sampler bindings, graphics state and command signatures
- **THEN** the complete renamed declarations and handle relationships are available without a native backend implementation

#### Scenario: Compile a consumer of unsupported extensions
- **WHEN** a consumer refers to ray tracing, meshlet, cooperative-vector, bindless, sparse, sampler-feedback or non-graphics queue declarations
- **THEN** those declarations remain present with their mapped signatures and fields even though no supporting backend is delivered by this capability

#### Scenario: Review declaration correspondence
- **WHEN** the public surface is compared with the fixed source using the recorded mapping and adaptation list
- **THEN** every public declaration has an accountable counterpart and no unexplained signature, field, default argument, qualifier, overload or declaration removal remains

### Requirement: Include public headers independently of native facilities
Each documented public header SHALL be self-contained on the supported Windows x64 MSVC C++20 toolchain and consumable without a precompiled header, an earlier include or native graphics/window/UI headers. The public consumer target SHALL provide its actual include root and C++20 requirement without depending on business modules, the reference snapshot or an unimplemented backend.

#### Scenario: Include each public entry by itself
- **WHEN** a translation unit includes exactly one documented public header before exercising its declarations
- **THEN** it compiles with the supported first-party diagnostic settings without relying on include order or private headers

#### Scenario: Consume the target from another project
- **WHEN** an independent build consumes only the public RHI target and uses header-provided behavior
- **THEN** the consumer compiles and links without manually adding project include roots, a backend SDK or reference-source dependencies

#### Scenario: Inspect dependency boundaries
- **WHEN** the configured public target and consumer compilation are inspected
- **THEN** they introduce no Core, Platform, Engine business, Editor, GLFW, GL loader, ImGui, Vulkan SDK or reference-NVRHI include/link dependency

### Requirement: Preserve public values and GPU-facing layouts
Documented descriptor fields, units, valid-input defaults, existing enum values and GPU-facing record layouts SHALL retain their fixed-source meaning. The original source header version SHALL remain identifiable as 27. Adding the OpenGL API identifier SHALL preserve all existing API identifiers and SHALL NOT imply binary ABI compatibility with the differently named upstream library.

#### Scenario: Inspect default descriptors and enums
- **WHEN** a consumer constructs descriptors with documented initialized defaults and inspects existing enum values
- **THEN** the defaults, units and numeric relationships match the fixed source apart from explicitly documented adaptations

#### Scenario: Compile GPU record layout checks
- **WHEN** the consumer verifies binding records, resource ranges and shader-visible indirect or instance records on the supported toolchain
- **THEN** their documented sizes, alignments and applicable field offsets or packed representations match the fixed-source contract

#### Scenario: Identify the OpenGL API addition
- **WHEN** a consumer inspects the API identifiers and source version metadata
- **THEN** OpenGL is distinct from the preserved existing identifiers and the originating header version can be determined without loading a device

### Requirement: Preserve intrusive CPU reference ownership
Public resource handles SHALL preserve the fixed-source intrusive ownership rules for retained raw pointers, copy, move, borrowed access, adoption, detachment and release. Compatible derived-to-base handle conversion SHALL retain its ownership meaning; incompatible conversion SHALL be rejected at compile time. CPU reference behavior SHALL NOT represent proof of GPU completion.

#### Scenario: Copy and release a retained handle
- **WHEN** a consumer retains a resource pointer, copies its handle and releases the handles in sequence
- **THEN** each owning copy contributes a reference and the resource is destroyed exactly once after the final owning reference is released

#### Scenario: Move or transfer an owned reference
- **WHEN** a consumer moves a handle or uses adoption and detachment according to their ownership preconditions
- **THEN** the reference is transferred without an extra owning reference, the moved-from or detached handle is empty and the adopted reference is released exactly once

#### Scenario: Borrow or convert a handle
- **WHEN** a consumer borrows the resource or converts a handle between compatible resource types
- **THEN** borrowed access adds no reference, compatible owning conversion preserves liveness and an incompatible implicit conversion fails to compile

### Requirement: Preserve fixed-capacity container interoperability
For inputs satisfying their documented size, index and element-operation preconditions, the public fixed-capacity containers SHALL retain their fixed-source content, iteration and capacity behavior. Their standard container aliases and operations SHALL remain usable by the standard library and range-based iteration. Container storage SHALL NOT silently grow beyond the declared capacity.

#### Scenario: Use empty and full valid containers
- **WHEN** a consumer performs supported operations on empty, partially filled and exactly full containers without violating operation preconditions
- **THEN** size, contents, iteration boundaries and capacity remain consistent and the container itself performs no dynamic storage growth

#### Scenario: Use standard algorithms and ranges
- **WHEN** a consumer iterates a container using const and mutable ranges and applies compatible standard algorithms
- **THEN** the documented iterator and container protocol remains available after project naming adaptation

#### Scenario: Swap partially filled scalar or pointer containers
- **WHEN** a consumer constructs partially filled scalar or pointer containers from initializer lists and swaps them within their capacity preconditions
- **THEN** backing slots accessed by the swap are initialized, logical contents and sizes are exchanged correctly and no dynamic storage growth is introduced

### Requirement: Compare and hash binding identities by semantic fields
Valid initialized binding keys SHALL distinguish resource identity, slot, array element, kind, view dimension, format, component mapping and the active range for that kind. Binding-set keys SHALL distinguish their liveness policy. Equality and hashing SHALL use the same semantic fields and SHALL NOT read inactive payloads, reserved storage or padding. Equal keys SHALL hash equally; collisions SHALL NOT make unequal keys equivalent.

#### Scenario: Distinguish array elements
- **WHEN** two otherwise identical bindings address different elements of the same descriptor array
- **THEN** they compare unequal and coexist as distinct keys in an unordered container

#### Scenario: Distinguish texture and buffer ranges
- **WHEN** initialized texture bindings differ in a mip/layer range or buffer bindings differ in a byte range
- **THEN** the keys compare according to those active semantic fields without reading a different union payload

#### Scenario: Distinguish liveness policies
- **WHEN** two binding-set descriptions contain the same bindings but request different automatic liveness policies
- **THEN** they compare unequal and cannot be substituted as the same key

#### Scenario: Ignore reserved storage
- **WHEN** initialized bindings have equal semantic fields but different reserved-field values
- **THEN** they compare equal and produce equal hashes without depending on padding bytes

#### Scenario: Encounter a hash collision
- **WHEN** an unordered container encounters equal hash values for unequal initialized binding keys
- **THEN** equality preserves their distinct identities and lookup returns the matching key

### Requirement: Trace the port to immutable source inputs
The delivered port SHALL preserve the required source copyright and license notices and record the fixed repository revision, source/target path correspondence, source content hashes, naming rules and each non-mechanical adaptation. The checked-in reference snapshot SHALL remain unchanged and outside the production build. A changed source input SHALL invalidate the recorded correspondence until deliberately reviewed.

#### Scenario: Audit a delivered header
- **WHEN** a maintainer follows a public header's provenance information
- **THEN** its fixed source, content hash, target mapping, applicable license and declared differences can be identified without an external service

#### Scenario: Detect source drift
- **WHEN** a source file no longer matches its recorded input hash
- **THEN** the port audit reports the mismatch instead of reporting that the unchanged mapping has passed

### Requirement: Distinguish declarations from implemented capabilities
The delivered capability SHALL identify which behavior is provided by headers and which declarations still require Common or backend definitions. It SHALL NOT satisfy missing implementations with successful no-ops, advertise a working device or infer support from enum/type availability. Its completion evidence SHALL preserve the separate pending native-window acceptance and later-stage graphics work.

#### Scenario: Check an external helper signature
- **WHEN** a consumer checks the signature of a declared helper whose implementation belongs to a later work package
- **THEN** the declaration remains available for compilation and the documented deliverable makes clear that the helper is not yet supplied as a linkable implementation

#### Scenario: Run the current product after the port
- **WHEN** the existing product is rebuilt and launched after public-contract delivery
- **THEN** it retains its existing native-host lifecycle and does not claim to have gained device creation, drawing, submission or presentation behavior from this capability

#### Scenario: Report work-package completion
- **WHEN** this capability's implementation and applicable checks have passed
- **THEN** completion is limited to the public-contract work package and does not close the pending cross-display DPI acceptance or either entire graphics-development stage
