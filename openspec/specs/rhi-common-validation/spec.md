# rhi-common-validation Specification

## Purpose

Define the linkable generic RHI behavior and validation boundary available before a native backend. Consumers can distinguish invalid requests, unsupported operations and real delegated results, while abstract state dependencies remain separate from native barriers and GPU completion.

## Requirements

### Requirement: Consume Physara domain namespace spellings
The delivered public contract SHALL use the project namespace spellings Physara::RHI::Utils, Physara::RHI::RT, Physara::RHI::RT::Cluster, Physara::RHI::CoopVec and Physara::RHI::Validation. Renaming SHALL preserve the corresponding declarations, data layouts, defaults and behavior, including standard-library integration. Previous lowercase source-domain spellings SHALL NOT be exposed as compatibility aliases.

#### Scenario: Compile consumers with the unified domain spellings
- **WHEN** a supported C++20 consumer uses the utility, ray-tracing, cluster, cooperative-vector and validation declarations through their Physara domain names
- **THEN** the declarations and applicable generic behavior are available with unchanged field layouts, values and standard hash integration

#### Scenario: Reject previous source-domain spellings
- **WHEN** a consumer attempts to use the previous lowercase RHI domain spellings
- **THEN** compilation rejects those names instead of exposing a parallel compatibility surface

### Requirement: Link the generic behavior behind the public contract
The supported C++20 consumer SHALL be able to link the delivered generic helpers, format metadata, range operations and validation entry point through the documented RHI target. Valid-input results SHALL retain the fixed-source meaning except for documented correctness adaptations. Header-version mismatch SHALL be observable without loading a device.

#### Scenario: Check the originating header version
- **WHEN** a consumer links and checks the originating version and a different version
- **THEN** the originating version succeeds and the mismatch fails without a native context

#### Scenario: Resolve whole and bounded ranges
- **WHEN** valid resource descriptions are used with a whole-resource sentinel, a partial range or an oversized count
- **THEN** generic range resolution produces the documented bounded intersection without arithmetic wraparound

#### Scenario: Inspect format metadata and utility results
- **WHEN** a consumer requests known format metadata, documented unknown-format fallback or a valid pure utility result
- **THEN** it receives the fixed-source data without interpreting metadata availability as device support

### Requirement: Allocate indices within a representable fixed capacity
The delivered index allocator SHALL issue only unallocated indices within its immutable capacity, report exhaustion and make released indices reusable. Zero capacity SHALL be valid and exhausted. Capacity that cannot be represented by the public index type SHALL throw std::length_error before storage allocation. The optional synchronized mode SHALL preserve uniqueness under concurrent allocation and release.

#### Scenario: Exhaust and reuse the capacity
- **WHEN** a consumer fills the declared capacity, allocates again, releases a valid allocation and retries
- **THEN** exhaustion is reported as -1 and the released capacity can be reused without silent growth

#### Scenario: Reject an unrepresentable capacity
- **WHEN** construction requests capacity beyond the public signed-index range
- **THEN** construction throws std::length_error before attempting the large allocation

#### Scenario: Use zero capacity or synchronized callers
- **WHEN** the capacity is zero or multiple callers use the synchronized mode with valid live allocations
- **THEN** zero capacity always reports exhaustion and concurrent callers never receive the same live index

### Requirement: Compose validation with explicit CPU ownership
A successfully created validation layer SHALL retain the supplied underlying device, forward accepted results without fabricating success and preserve the documented borrowed callback lifetime. A missing device or required callback SHALL yield no usable layer. Invalid and unsupported requests SHALL be distinguishable through the existing failure result and operation-specific diagnostics. CPU retention SHALL NOT imply GPU completion.

#### Scenario: Reject missing construction dependencies
- **WHEN** layer creation receives a null device or a device with no required callback
- **THEN** it returns an empty result before creating a partial wrapper or issuing device work

#### Scenario: Retain and release the wrapped device
- **WHEN** callers release their device reference while a layer or its command list remains alive and later release the remaining owners
- **THEN** the underlying CPU device remains alive for its actual borrowers and its final owning release occurs exactly once

#### Scenario: Preserve an underlying failure
- **WHEN** an otherwise accepted delegated operation returns an empty handle, false or zero submission result
- **THEN** the layer preserves that failure and does not invent a resource, positive result or completion ticket

### Requirement: Reject unsafe inputs before device work
Validation SHALL reject invalid initialized descriptors, incompatible live resource types, null required inputs and unrepresentable or out-of-bounds ranges in Debug and Release before the affected operation reaches the underlying device. Checks SHALL NOT rely on wrapped arithmetic, inactive payloads or diagnostic-only assertions. Caller-owned pointers SHALL still satisfy their documented lifetime and array-storage preconditions.

#### Scenario: Reject a wrong resource type or required null
- **WHEN** an initialized binding carries a live resource of an incompatible type or an operation omits a required resource or data pointer
- **THEN** validation reports the operation and invalid input without an unsafe cast, dereference or underlying operation

#### Scenario: Reject an overflowing buffer or heap interval
- **WHEN** an offset and requested byte count would wrap or extend beyond the supplied resource capacity
- **THEN** validation rejects the interval even when unchecked addition would appear to fit

#### Scenario: Validate texture descriptions and subresources
- **WHEN** dimensions, format, sample or mip/layer inputs violate their semantic contract or cannot be represented for safe indexing
- **THEN** validation rejects them before shifting, multiplying, resolving or forwarding an invalid resource operation

#### Scenario: Validate binding arrays and views
- **WHEN** binding type, slot, array element, view format, active range or layout correspondence is invalid
- **THEN** validation rejects the binding while preserving distinct valid array elements and separate texture/sampler identities

### Requirement: Distinguish API policy from underlying capabilities
The layer SHALL identify OpenGL explicitly and keep positive feature/format support grounded in permitted underlying queries. In the initial OpenGL policy, only the existing constant-buffer-range feature can pass through as supported; other known features report false. Metadata, enum presence and CPU fixtures SHALL NOT establish native support. Unsupported queries SHALL NOT overwrite caller output data.

#### Scenario: Identify OpenGL without another API fallback
- **WHEN** a consumer inspects a wrapped OpenGL identity and its printable API name
- **THEN** both identify OpenGL and no D3D or Vulkan identity is substituted

#### Scenario: Filter unsupported feature queries
- **WHEN** the underlying device advertises an initial-policy feature that OpenGL does not permit
- **THEN** the layer reports false without forwarding a forbidden positive query or overwriting its output data

#### Scenario: Delegate a permitted capability query
- **WHEN** the initial policy permits a query and the underlying device reports support or lack of support
- **THEN** the layer preserves the underlying result rather than hardcoding positive support

#### Scenario: Separate format metadata from support
- **WHEN** a format has valid metadata but the underlying format-support query lacks a required usage
- **THEN** validation or format selection rejects that usage instead of treating the format table as capability evidence

### Requirement: Reject operations outside the initial OpenGL policy
OpenGL operations outside the documented initial policy SHALL fail visibly before unsupported work or unsafe argument inspection. Non-graphics queues, deferred lists, bindless, virtual/heap-backed resources, RT, meshlet, sampler-feedback, CoopVec and other excluded extensions SHALL retain their public declarations. An unsupported reference-only crash-helper access SHALL report the reason and throw std::logic_error rather than return a fake object.

#### Scenario: Reject queues and deferred recording
- **WHEN** a caller requests a non-graphics queue, cross-queue wait or deferred command list through the initial OpenGL policy
- **THEN** the call is rejected with its existing failure form or unsupported diagnostic before delegation

#### Scenario: Reject an unsupported extension family
- **WHEN** a caller attempts any creation, mutation, query or command in an excluded extension family
- **THEN** the family is consistently refused without a successful no-op or a change to the public declaration surface

#### Scenario: Refuse a void operation without inspecting unsupported payloads
- **WHEN** a void command outside the initial policy is invoked with payloads the unsupported path does not need
- **THEN** validation emits an unsupported diagnostic and returns before dereferencing those payloads or forwarding the command

#### Scenario: Refuse the unsupported reference-only helper
- **WHEN** an OpenGL caller requests the crash helper while that facility is unavailable
- **THEN** the reason is reported and std::logic_error is thrown without returning an invalid reference or introducing an SDK dependency

### Requirement: Enforce command-list ownership and lifecycle
Validation SHALL reject commands outside their open-state precondition, overlapping immediate recordings and submission of foreign, unwrapped, duplicated or wrongly queued lists. Immediate lists SHALL require close and submission before reuse. A batch rejected during validation SHALL leave every list's prior lifecycle state unchanged and SHALL NOT call the underlying submission operation.

#### Scenario: Follow the immediate lifecycle
- **WHEN** a caller opens, records, closes and submits a valid immediate list before opening it again
- **THEN** each accepted transition is forwarded once and no synthetic completion is introduced

#### Scenario: Reject invalid recording transitions
- **WHEN** a caller records before open, opens an already open list, closes twice, reopens a closed unsubmitted immediate list or overlaps two immediate recordings
- **THEN** validation reports the invalid state without changing another list's ownership or issuing the rejected operation

#### Scenario: Reject a batch without partially consuming it
- **WHEN** a later element in a submission batch fails validation after an earlier element passed its checks
- **THEN** no list is consumed and a corrected batch can still use the earlier closed list

#### Scenario: Reject foreign or duplicated list identities
- **WHEN** a batch contains a list from another validation device, an unwrapped list or the same list more than once
- **THEN** validation rejects the entire batch before submission and preserves the original list states

### Requirement: Produce abstract resource-state dependencies
The generic tracker SHALL preserve fixed-source valid-input state semantics for whole resources and subresources, initial-state restoration, permanent states and UAV ordering. Beginning tracking SHALL declare a known state without issuing a transition. State requests SHALL produce inspectable abstract dependency records for one later backend emitter; they SHALL NOT issue native barriers or prove GPU completion.

#### Scenario: Declare a known state before requiring a new use
- **WHEN** a caller begins tracking with a valid known state and later requests another valid use
- **THEN** the declaration itself adds no transition and the request records the required abstract dependency

#### Scenario: Track whole resources and individual subresources
- **WHEN** requests cover a whole texture or selected valid mip/layer ranges
- **THEN** recorded prior/final states and affected subresources remain consistent without changing unrelated subresources

#### Scenario: Order repeated UAV access
- **WHEN** repeated unordered access requires an ordering dependency under the current UAV policy
- **THEN** the tracker retains that dependency even when the abstract state value does not change

#### Scenario: Restore and publish state metadata at the right stage
- **WHEN** recording is closed and a valid submission is later accepted by its owner
- **THEN** required initial-state restoration is available before submission and permanent/initialized metadata is published only at accepted submission, without marking the GPU complete

### Requirement: Expose invalid tracking and discard abandoned recordings
An unknown prior state, invalid range, unsupported state or conflicting permanent-state request SHALL make the recording error observable and prevent its dependencies from being treated as valid work. Discarding an erroneous or abandoned recording SHALL release its local records without publishing permanent or initialized resource metadata. A fresh recording SHALL begin without stale state or retained resource pointers.

#### Scenario: Reject an unsafe tracking request
- **WHEN** a request has an unknown prior state, invalid subresource interval or conflicting permanent state
- **THEN** the recording exposes an error and cannot publish that request as a valid dependency sequence

#### Scenario: Discard after failure or cancellation
- **WHEN** an owner discards a recording with pending state changes
- **THEN** pending dependencies and borrowed resource records are released and external state metadata remains unchanged

#### Scenario: Reuse a tracker after discard
- **WHEN** a discarded tracker is used for a new valid recording
- **THEN** its result depends only on the new declarations and current resource metadata

### Requirement: Deliver a traceable generic implementation without native claims
The implementation SHALL preserve fixed inputs, source/target correspondence, required licenses and enumerated adaptations, and remain independently consumable without reference sources or native SDKs. Its completion SHALL be limited to Common/Validation and CPU evidence. Real GL device wrapping, caps and resource-ownership/native behavior checks SHALL remain assigned to the later native work packages; the pending D01 acceptance SHALL remain visible.

#### Scenario: Build a consumer without a backend
- **WHEN** an independent supported build links only the documented RHI target and uses the delivered generic behavior
- **THEN** Debug and Release compile and link without Core, Platform, graphics/window/UI SDKs, a reference-source target or an auxiliary product entry

#### Scenario: Audit fixed-source correspondence
- **WHEN** a maintainer checks the retained source identities, naming and adapted implementation
- **THEN** all delivered declarations and behaviors have accountable counterparts and changed inputs or unexplained removals are detected

#### Scenario: Report completion without closing native work
- **WHEN** the implementation and its CPU/build checks have passed
- **THEN** the work package is reported complete only under the accepted staged boundary, with native integration, real GPU semantics and D01/D02 exits still pending
