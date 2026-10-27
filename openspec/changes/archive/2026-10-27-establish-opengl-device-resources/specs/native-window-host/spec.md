# Spec Delta

## MODIFIED Requirements

### Requirement: Create a verified native OpenGL context
Successful visible or hidden host creation SHALL require a real current OpenGL 4.6+ Core context, required entry points, a double-buffered default framebuffer and its resource device with validation ready. Invalid configuration, missing native or RHI requirements and creation failures SHALL return distinguishable failures without a partially ready host.

#### Scenario: Create a supported visible or hidden host
- **WHEN** a caller creates a host on a supported system with a valid initial logical size and either visibility setting
- **THEN** success identifies a verified current context and the selected visibility without changing the required graphics profile

#### Scenario: Reject insufficient graphics facilities
- **WHEN** native creation fails, the actual context is below the required version or profile, or a required entry point or default framebuffer property is unavailable
- **THEN** creation fails with the relevant operation and reason and returns no usable host

#### Scenario: Reject invalid startup data
- **WHEN** the initial logical dimensions are zero or cannot be represented by the native size parameters, or the title contains an embedded null
- **THEN** creation rejects the configuration before changing process native state

#### Scenario: Borrow the resource device after readiness
- **WHEN** creation succeeds and the caller borrows the device on the owner thread
- **THEN** it receives the live validated device without gaining ownership of the host, context or device reference

#### Scenario: Fail during native resource device creation
- **WHEN** context creation succeeds but a required resource entry, device query or device allocation fails
- **THEN** the host reports the RHI creation failure, preserves available native error context and unwinds the device, context, window and process state

#### Scenario: Fail during validation composition
- **WHEN** native device creation succeeds but composing the required validation layer fails
- **THEN** no Ready host is returned and the unpublished device and native host are released in reverse dependency order

### Requirement: Release native ownership completely and repeatably
On the owner thread, shutdown and rollback SHALL release acquired state in reverse dependency order, disconnect borrowed callbacks before storage dies and restore the prior process error callback. Successful shutdown SHALL be idempotent and allow a new host; a stopped owner SHALL NOT release a newer host. Shutdown with external RHI owners or live resources SHALL instead report ResourceInUse before teardown and permit retry after release.

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

#### Scenario: Reject shutdown while a resource is live
- **WHEN** explicit shutdown is requested while an externally owned buffer, texture, sampler or framebuffer remains alive
- **THEN** it reports ResourceInUse before destroying the device, callbacks, input or native environment and a later retry can succeed after those resources are released

#### Scenario: Reject shutdown while the device is externally retained
- **WHEN** explicit shutdown is requested while a caller owns an extra device or validation-layer reference
- **THEN** it reports ResourceInUse without invalidating that reference or its callback target and permits retry after the reference is released

#### Scenario: Drain final releases before closing the context
- **WHEN** resource users have stopped, final releases have been queued and no external RHI owner remains
- **THEN** shutdown drains native deletions with the owning context current, releases validation before its raw device and releases the callback target after device destruction
