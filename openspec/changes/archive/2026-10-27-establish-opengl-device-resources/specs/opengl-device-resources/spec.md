# Spec Delta

## Purpose

Define the first native OpenGL resource capability behind Physara's public RHI. Callers can create, inspect and release basic device-owned resources under real context limits without mistaking allocation, format metadata or CPU ownership for drawing, submission or GPU completion.

## ADDED Requirements

### Requirement: Create a device for the owning current context
Device creation SHALL require the recorded owner thread, its current verified context, required entry points and a live diagnostic target. Successful creation SHALL identify OpenGL and establish the adopted upper-left, zero-to-one clip convention. Failure SHALL expose no usable or partially initialized device.

#### Scenario: Create through a real visible or hidden host
- **WHEN** either supported host visibility path completes native device creation
- **THEN** its borrowed device identifies OpenGL and actual clip-control queries confirm the adopted origin and depth range

#### Scenario: Reject missing context or required entries
- **WHEN** creation lacks its required context, callback, context check or a required resource entry point
- **THEN** it fails with an operation-specific reason without publishing a device or issuing resource allocation

#### Scenario: Reject an operation outside the native owner
- **WHEN** a native device operation is attempted on another thread or while another context is current
- **THEN** it fails before performing the rejected OpenGL operation and does not silently move the context

### Requirement: Expose actual resource capabilities conservatively
Reported resource support SHALL be the intersection of the implemented resource profile and actual device, target, format and usage observations. Creation SHALL enforce the same limits, including supported sample counts rather than only a maximum. Metadata and allocation support SHALL NOT imply an implemented shader, binding or draw path. Unavailable feature queries SHALL preserve caller output.

#### Scenario: Query mapped and unmapped formats
- **WHEN** a caller queries a mapped format, a known but excluded format or an unknown value
- **THEN** only supported mapped usages receive positive flags and excluded or unknown formats report no support

#### Scenario: Reject a sample count absent from the supported set
- **WHEN** a requested multisample count is absent from the actual format and target's permitted sample set
- **THEN** creation rejects it even if the count is below a reported global maximum

#### Scenario: Enforce actual dimensions and attachment limits
- **WHEN** a resource exceeds an applicable device dimension, layer, representable size or attachment limit
- **THEN** it fails before native allocation rather than truncating or clamping the request

#### Scenario: Query a feature without a delivered operation
- **WHEN** the underlying API has facilities for a feature whose RHI operation is not implemented in this profile
- **THEN** the device and its validation layer report no enabled support and leave optional output data unchanged

### Requirement: Create immutable buffer allocations
A valid supported buffer description SHALL produce one device-owned immutable allocation with the requested byte capacity and preserved descriptor meaning. Size conversions and layout constraints SHALL be checked before allocation. CPU mapping, volatile storage and typed views outside this profile SHALL be rejected. Allocating storage SHALL NOT initialize its contents or represent a completed upload.

#### Scenario: Create and inspect an ordinary buffer
- **WHEN** a caller requests a supported nonzero buffer allocation
- **THEN** the returned descriptor and native allocation query agree on the requested byte capacity and immutable storage

#### Scenario: Reject an unrepresentable buffer allocation
- **WHEN** the requested capacity is zero, cannot be represented by native allocation parameters or violates a declared structured or raw layout
- **THEN** creation reports the invalid request before allocating a native object

#### Scenario: Request unavailable buffer access
- **WHEN** a caller requests CPU mapping, volatile storage or a typed buffer view outside this profile
- **THEN** the request fails explicitly without substituting ordinary storage or reporting a successful upload

### Requirement: Create texture storage with explicit shape and usage
Valid supported textures SHALL retain their dimensions, mip and array semantics, sample count, usage and color encoding. Storage and creation checks SHALL use the actual target and format. Unsupported usages or formats SHALL fail without conversion to another resource. Initial-state and clear-value metadata SHALL NOT be interpreted as initialized pixels or completed transitions.

#### Scenario: Allocate supported texture dimensions
- **WHEN** a caller creates valid one-dimensional, two-dimensional, three-dimensional, array, cube or permitted multisample storage
- **THEN** native storage queries match the effective mip dimensions, array or face count, sample count and sized format

#### Scenario: Reject an incompatible shape
- **WHEN** a request has an invalid cube face count, incompatible dimension fields, excessive mips or an invalid sample and mip combination
- **THEN** it is rejected before native storage creation

#### Scenario: Keep data and sRGB storage distinct
- **WHEN** otherwise comparable textures request supported linear data and sRGB formats
- **THEN** their native storage and reported format retain that distinction without modifying bytes or silently selecting another encoding

#### Scenario: Preserve uninitialized-content semantics
- **WHEN** storage is created with initial-state or clear-value metadata and no content operation has occurred
- **THEN** the resource is not reported as containing uploaded or cleared pixels and no state transition or GPU completion is invented

### Requirement: Keep view interpretation compatible with storage
An accepted attachment view SHALL address the requested valid subresources and a permitted interpretation of its parent's immutable storage. Reinterpretation SHALL require the declared typeless policy and actual view compatibility. Explicit invalid ranges SHALL fail rather than becoming bounded intersections. Unavailable public native-view requests SHALL return no view without fabricating an object.

#### Scenario: Create a bounded attachment view
- **WHEN** a supported attachment selects a valid mip and single layer or cube face
- **THEN** its native attachment identifies exactly that subresource and the resource keeps its original storage

#### Scenario: Accept or reject a format reinterpretation
- **WHEN** an attachment requests a different view format
- **THEN** only a typeless resource with a supported compatible interpretation succeeds and an incompatible or typed-only request fails before creating the view

#### Scenario: Reject unavailable native-view exports
- **WHEN** a caller requests a native view or object kind outside the documented backend interoperability surface
- **THEN** the query returns no object and does not invent a handle or mutate the texture's interpretation

### Requirement: Keep texture and sampler state independent
Supported sampler creation SHALL preserve filter, address, finite bias, border and reduction semantics within actual device limits. Comparison sampling SHALL retain the documented fixed-source comparison meaning. Creating or releasing samplers SHALL NOT modify a texture's sampling state. Unsupported reductions or out-of-profile values SHALL fail explicitly.

#### Scenario: Create distinct samplers for the same prospective texture
- **WHEN** callers create two valid samplers with different filters or address modes
- **THEN** their native parameters differ as requested and existing texture parameters remain unchanged

#### Scenario: Inspect comparison semantics
- **WHEN** a caller creates a supported comparison sampler
- **THEN** its native comparison mode and comparison operation match the documented conventional shadow-depth contract rather than inheriting main-view Reverse-Z

#### Scenario: Reject an invalid or unsupported sampler
- **WHEN** parameters are nonfinite, exceed the declared device range or request an excluded reduction
- **THEN** creation returns no sampler with the operation and reason available and no fallback parameter clamping

### Requirement: Compose valid framebuffer attachments
Framebuffer creation SHALL validate the complete attachment set before publishing a native object. Accepted attachments SHALL have compatible extents and sample counts, valid roles and supported single-mip, single-layer views. Framebuffer completeness SHALL be confirmed. Descriptors and compatibility information SHALL describe the selected views without changing draw or read framebuffer bindings.

#### Scenario: Create a color or depth framebuffer
- **WHEN** a caller supplies a supported color-only, depth-only or matching combined attachment set
- **THEN** creation yields a complete framebuffer with dimensions, formats and samples matching the selected mip and layer

#### Scenario: Reject an incompatible attachment set
- **WHEN** an attachment has a wrong role, incompatible dimensions or samples, invalid range or exceeds the device attachment count
- **THEN** the entire request fails without leaving a published partial framebuffer or altering existing framebuffer bindings

#### Scenario: Reject an excluded attachment mode
- **WHEN** a caller requests an attachment-free framebuffer, layered output, a three-dimensional attachment or a shading-rate attachment outside this profile
- **THEN** creation reports the unsupported mode without flattening or replacing the attachment request

#### Scenario: Fail the native completeness check
- **WHEN** native framebuffer setup does not produce a complete object
- **THEN** creation reports the actual failure and releases every unpublished framebuffer and view acquired for that attempt

### Requirement: Reject foreign resources before native work
Resources accepted by a native device operation SHALL belong to that device and remain alive for the operation. Live objects of another implementation or device SHALL be rejected before native use, even through validation. Device and context ownership SHALL be checked directly rather than inferred from a reused native name or a global address registry.

#### Scenario: Supply another device's texture
- **WHEN** a framebuffer request includes a live texture belonging to a different device
- **THEN** the request fails before native framebuffer or attachment creation and leaves both owners usable

#### Scenario: Supply an incompatible live implementation
- **WHEN** a supported operation receives a live resource of the wrong type or implementation
- **THEN** it reports an invalid resource without relying on a Debug-only cast or touching its native object

#### Scenario: Encounter a reused native name
- **WHEN** a deleted resource name is later reused for a distinct live resource
- **THEN** ownership and attachment decisions use the current resource identity and do not accept stale state from the earlier lifetime

### Requirement: Preserve resource dependency ownership
Returned resource handles SHALL preserve intrusive CPU ownership. Framebuffers SHALL retain their texture dependencies and any native views they own until those attachments are retired. Resources SHALL retain their device while alive. Releasing a caller's texture handle SHALL NOT invalidate a live framebuffer. These references SHALL NOT be described as proof of GPU completion.

#### Scenario: Release an attachment's caller handle
- **WHEN** a caller releases its texture handle while a framebuffer still uses the texture
- **THEN** the attachment remains alive and inspectable until the framebuffer releases its dependency

#### Scenario: Release the final resource owner
- **WHEN** the final owning CPU handle for a resource is released
- **THEN** its wrapper is destroyed once and its native allocation is retired exactly once under the documented context protocol

#### Scenario: Retain the host's borrowed device deliberately
- **WHEN** a caller creates an owning handle from the host's borrowed device
- **THEN** that handle contributes a real reference which must be released before successful host shutdown

### Requirement: Delete native objects on the owning context thread
Final CPU release SHALL require no new retirement allocation and SHALL issue no GL deletion on a foreign thread. Valid-context deletion SHALL run on the owner before device and context release. Unusable contexts SHALL follow loss cleanup without further GL calls or invented completion. Retirement SHALL drain without fixed frame delays or an unbounded cache.

#### Scenario: Release the last handle on a worker
- **WHEN** a worker releases the final CPU handle of an otherwise unused resource
- **THEN** no GL call occurs on that worker and the owning thread can subsequently delete the native allocation exactly once

#### Scenario: Drain after framebuffer retirement
- **WHEN** framebuffer retirement releases the last attachment reference
- **THEN** framebuffer and owned view deletion precede dependent storage deletion and draining finishes without stale resources

#### Scenario: Retire while new allocations are disabled
- **WHEN** the final handle is released under a controlled CPU allocation failure
- **THEN** retirement still records the resource for deletion without allocating or leaking the native object

#### Scenario: Reject collection without the current context
- **WHEN** collection is invoked outside the owning current context
- **THEN** it reports the violated precondition without deleting objects or publishing a false completion

#### Scenario: Release after confirmed context loss
- **WHEN** the owning context is confirmed unusable and resource users have released their owners
- **THEN** cleanup releases CPU retirement state through the loss path without more GL work or a false completion value and the host releases the lost native environment

### Requirement: Preserve resource creation failures transactionally
Creation SHALL publish a handle only after all CPU and native steps succeed. Failures SHALL release unpublished state, retain operation context and any actual native error, and preserve the primary failure over cleanup diagnostics. A fault preventing reliable continued use SHALL stop new native work until controlled shutdown. No exception SHALL escape a native callback.

#### Scenario: Fail after a native object is acquired
- **WHEN** a controlled failure occurs after a name or storage has been acquired but before publication
- **THEN** the attempt returns no resource and releases all state acquired by that attempt on the owning context

#### Scenario: Fail CPU preparation
- **WHEN** required wrapper, descriptor or retirement-storage preparation fails
- **THEN** creation returns no partial resource through its documented failure boundary and leaves existing resources unchanged

#### Scenario: Encounter a secondary cleanup failure
- **WHEN** cleanup reports another failure after resource creation has failed
- **THEN** the original operation and native error remain distinguishable and additional diagnostics remain bounded

#### Scenario: Stop after an unrecoverable native fault
- **WHEN** a native failure makes continued device use unreliable
- **THEN** new resource work is rejected and controlled cleanup follows the actual context condition without inferring device loss solely from diagnostic severity

### Requirement: Reject unavailable operations without fictitious GPU work
The delivered resource-only device SHALL preserve the complete public interface while explicitly refusing unavailable content, shader, binding, draw, query, submission and extension operations. Unsupported calls SHALL preserve output data where required and SHALL NOT create successful dummy objects, upload data, issue a ticket or wait for invented GPU work.

#### Scenario: Request a command list or submission
- **WHEN** a caller requests an unavailable list or executes a nonempty batch on this resource-only device
- **THEN** the operation fails with no list, replay, issued work or positive submission result

#### Scenario: Request an excluded family with unused payload
- **WHEN** an unsupported extension operation is called with payload it does not need to inspect
- **THEN** it reports unsupported behavior before dereferencing the payload or forwarding native work

#### Scenario: Request an unavailable reference-only helper
- **WHEN** a caller requests the unsupported crash helper
- **THEN** the reason is reported and the documented logic-error failure occurs without returning a fake reference

### Requirement: Consume native and generic capabilities independently
The existing generic RHI target and headers SHALL remain independently consumable without a GL loader, window library, native SDK or business dependency. Native integration SHALL use the documented backend consumer entry and retain one-direction dependencies. Neither consumer SHALL require the fixed reference sources to be compiled or introduce a second product entry.

#### Scenario: Rebuild the independent generic consumer
- **WHEN** an independent supported build consumes only the generic target after native backend integration
- **THEN** generic Debug and Release behavior still compiles and links without native include or link dependencies

#### Scenario: Build and run the native consumer
- **WHEN** a supported consumer creates resources through the documented host and backend path
- **THEN** it uses the formal backend and retained loader sources without manually reconstructing native implementation or include paths

#### Scenario: Inspect first-party interface and dependency changes
- **WHEN** the integration and public headers are reviewed
- **THEN** native types remain inside interoperability boundaries, existing public values and layouts remain intact and the reference snapshot stays outside the build

### Requirement: Report allocation separately from completed rendering
Delivery SHALL identify the tested resource profile, native environment and observed results. Allocation, queries and CPU validation SHALL NOT imply upload, raster output, synchronization, presentation or whole-stage acceptance. Deferred acceptance and any prerequisite exception SHALL remain explicit. An approved exception SHALL stay limited to its documented work package and SHALL NOT close deferred acceptance.

#### Scenario: Report successful resource verification
- **WHEN** the resource checks and affected builds pass
- **THEN** completion identifies this resource capability and leaves content upload, full data and row-order readback, drawing and submission assigned to their remaining work packages

#### Scenario: Encounter an unmet implementation prerequisite
- **WHEN** D01 still lacks its required cross-display acceptance or stage exit
- **THEN** only an explicitly approved exception permits the documented resource work, while D01 acceptance and all other work-package prerequisites remain incomplete without relying on the earlier generic-only exception
