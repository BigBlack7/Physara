# native-window-host Specification

## Purpose

Define the native window host that later input, runtime and rendering work can share. This capability establishes observable creation, environment reporting, identity, timing, diagnostic and release behavior before the project claims a working RHI device or rendered frame.

## Requirements

### Requirement: Create a verified native OpenGL context
The host SHALL report successful creation only after obtaining a real, current OpenGL context with version 4.6 or later, a Core profile, the entry points required by this capability and a double-buffered default framebuffer. Visible and hidden windows SHALL use the same creation and validation path. Invalid configuration, unavailable native facilities and missing requirements SHALL produce distinguishable failures without a partially ready host.

#### Scenario: Create a supported visible or hidden host
- **WHEN** a caller creates a host on a supported system with a valid initial logical size and either visibility setting
- **THEN** success identifies a verified current context and the selected visibility without changing the required graphics profile

#### Scenario: Reject insufficient graphics facilities
- **WHEN** native creation fails, the actual context is below the required version or profile, or a required entry point or default framebuffer property is unavailable
- **THEN** creation fails with the relevant operation and reason and returns no usable host

#### Scenario: Reject invalid startup data
- **WHEN** the initial logical dimensions are zero or cannot be represented by the native size parameters, or the title contains an embedded null
- **THEN** creation rejects the configuration before changing process native state

### Requirement: Report owned observations of the native environment
A successful host SHALL expose an owned value snapshot of its identity, actual graphics version and profile, vendor and renderer strings, declared startup limits, default framebuffer color encoding and creation duration in seconds. Requested swap interval SHALL be identified as a request rather than proof of effective presentation behavior. An encoding observation SHALL NOT be inferred solely from a creation preference. These observations SHALL NOT imply unimplemented resource, drawing or presentation support.

#### Scenario: Inspect an actual environment
- **WHEN** creation succeeds and the caller inspects the environment snapshot
- **THEN** the reported graphics properties and limits originate from native queries and the snapshot identifies the values and their units

#### Scenario: Observe a linear default framebuffer
- **WHEN** native queries report linear color encoding despite a preference for an sRGB-capable framebuffer
- **THEN** the snapshot reports linear encoding and does not claim sRGB presentation has been validated

#### Scenario: Retain a copied snapshot
- **WHEN** the caller retains a copy of the environment snapshot and then closes the host
- **THEN** its strings and values remain readable without referencing destroyed native storage

### Requirement: Enforce exclusive ownership and the owner thread
Creation SHALL have the process main thread as a documented precondition. Native host operations SHALL remain on the recorded creation thread. The implementation SHALL allow at most one active or initializing host per process and SHALL reject a second claim before changing the first host's native state. An explicit native operation on an existing host from another thread SHALL fail before performing native work.

#### Scenario: Reject a second live host
- **WHEN** a second creation is attempted while another host is initializing or ready
- **THEN** the attempt reports exclusive ownership failure and leaves the first host, its current context and its callbacks intact

#### Scenario: Reject explicit shutdown on another thread
- **WHEN** a worker calls explicit shutdown on a host owned by the main thread
- **THEN** shutdown reports a thread violation without destroying the window or releasing the owner's process claim

### Requirement: Preserve contextual failures across rollback
Creation and explicit shutdown failures SHALL carry a stable domain reason, operation stage, owned bounded description, source context and native error code when one exists. A creation attempt identity SHALL be included once allocated. Rollback and diagnostic failures SHALL NOT overwrite the original creation failure. Exceptions SHALL NOT escape through native callbacks.

#### Scenario: Preserve an error description
- **WHEN** a native error is reported using a temporary description and the native operation returns
- **THEN** the returned failure retains the description within its declared bound without borrowing the callback's storage

#### Scenario: Fail after acquiring native state
- **WHEN** an operation fails after library initialization, window creation or context activation
- **THEN** the acquired state is unwound, no successful host is returned, and the original stage and reason remain available

#### Scenario: Encounter another error during rollback
- **WHEN** rollback reports an additional failure
- **THEN** the primary creation failure remains distinguishable and the additional cleanup failure is observable without unbounded error accumulation

### Requirement: Release native ownership completely and repeatably
Shutdown and failed creation SHALL release all state acquired by this capability in reverse dependency order on the owner thread. Owned callbacks and borrowed targets SHALL be disconnected before their storage is released, and a previously installed process error callback SHALL be restored. Successful shutdown SHALL be idempotent. After successful rollback or shutdown, the same process SHALL be able to create a new host. A stopped old owner SHALL NOT release the resources or process claim of a newer host.

#### Scenario: Repeat successful create and shutdown
- **WHEN** a caller repeatedly creates a real host, explicitly shuts it down and destroys the stopped owner
- **THEN** each iteration releases its window, context and process ownership and the next iteration can initialize successfully

#### Scenario: Retry after failed creation
- **WHEN** a controlled creation failure has been rolled back successfully and the caller then makes a valid request
- **THEN** the valid request can create a real host without stale initialization or callback state

#### Scenario: Restore callback ownership
- **WHEN** a callback existed before the host acquired native ownership and the host subsequently shuts down or rolls back
- **THEN** that callback is restored and later native errors cannot access the destroyed host's error storage

#### Scenario: Shut down a stopped owner after replacement
- **WHEN** a stopped owner remains alive, a new host is created, and shutdown is called again on the old owner
- **THEN** the operation is an idempotent success and leaves the new host intact

### Requirement: Keep identities distinct across types and lifetimes
The foundation SHALL provide identities that cannot be implicitly mixed between domains. Host identities SHALL distinguish successive lifetimes even if a logical slot is reused. Generation exhaustion SHALL be reported rather than wrapping into an older identity. Native pointers SHALL NOT serve as the public host identity.

#### Scenario: Reject another identity domain
- **WHEN** a consumer attempts to pass an identity from a different domain where a host identity is required
- **THEN** the mismatch is rejected at compile time without an explicit conversion

#### Scenario: Reuse a logical host slot
- **WHEN** a host is released and a later host uses the same logical slot
- **THEN** the later host has a different generation and the earlier identity does not identify the active host

#### Scenario: Exhaust the generation range
- **WHEN** allocating another lifetime would wrap the generation range
- **THEN** identity allocation fails without creating native state or reissuing an earlier identity

### Requirement: Measure elapsed time in monotonic seconds
Startup durations and diagnostic elapsed timestamps SHALL use a monotonic CPU clock and expose seconds explicitly. Their availability SHALL NOT depend on successful native library initialization. CPU timing SHALL NOT be represented as GPU completion or presentation timing.

#### Scenario: Time successful and failed initialization
- **WHEN** an initialization attempt succeeds or fails before native initialization is complete
- **THEN** its measured elapsed duration is finite, nonnegative and expressed in seconds

### Requirement: Bound diagnostic retention and expose loss
Diagnostic retention SHALL have declared record and per-message byte limits, preserve ownership of retained text and remain within those limits under repeated or oversized input. Eviction, truncation and write failures SHALL be observable through bounded status or counters. Diagnostic failures SHALL NOT replace a native operation's primary error or escape through a native callback.

#### Scenario: Exceed the retained record limit
- **WHEN** more records are written than the declared retention capacity
- **THEN** retained records stay within that capacity, the documented oldest-record eviction policy is followed and the eviction count is observable

#### Scenario: Submit an oversized message
- **WHEN** a message exceeds the declared byte limit
- **THEN** retained text stays within the limit, valid UTF-8 input remains valid after truncation, and truncation is indicated

#### Scenario: Fail to retain a diagnostic
- **WHEN** a diagnostic backend cannot retain a record
- **THEN** the write failure is observable and native operation error reporting continues independently
