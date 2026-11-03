# Spec Delta

## MODIFIED Requirements

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

#### Scenario: Reject a descriptor-set index beyond pipeline capacity
- **WHEN** a pipeline layout interprets its register space as a descriptor-set index outside the supported layout capacity
- **THEN** validation reports the invalid index in Debug and Release without out-of-bounds memory access or underlying pipeline creation, while valid boundary indices preserve their existing delegated result

### Requirement: Expose invalid tracking and discard abandoned recordings
An unknown prior state, invalid range, unsupported state, conflicting permanent-state request or local tracking-storage failure SHALL make the recording error observable and prevent its dependencies from being treated as valid work. Discarding an erroneous or abandoned recording SHALL release its local records without publishing permanent or initialized resource metadata. A fresh recording SHALL begin without stale state or retained resource pointers. A local recording failure SHALL NOT release resources still used by an already issued immediate-upload prefix before that prefix is safely retired.

#### Scenario: Reject an unsafe tracking request
- **WHEN** a request has an unknown prior state, invalid subresource interval or conflicting permanent state
- **THEN** the recording exposes an error and cannot publish that request as a valid dependency sequence

#### Scenario: Discard after failure or cancellation
- **WHEN** an owner discards a recording with pending state changes
- **THEN** pending dependencies and borrowed resource records are released and external state metadata remains unchanged

#### Scenario: Reuse a tracker after discard
- **WHEN** a discarded tracker is used for a new valid recording
- **THEN** its result depends only on the new declarations and current resource metadata

#### Scenario: Observe allocation failure during first texture tracking or expansion
- **WHEN** local storage cannot be allocated during first texture tracking or expansion to individual subresources
- **THEN** the recording exposes a persistent error without terminating the process or publishing resource metadata, and explicit discard allows a later valid recording

#### Scenario: Preserve an issued upload prefix after local tracking failure
- **WHEN** an immediate upload has issued valid work and a later texture tracking allocation fails
- **THEN** the owner retains the prefix's resource and upload-storage references until actual completion or its documented controlled exit, without reporting the failed recording as a successful submission

## ADDED Requirements

### Requirement: Preserve ordinary CPU allocation failure boundaries
Ordinary Validation CPU storage failures during pipeline and texture validation SHALL follow the documented recoverable exception boundary and SHALL NOT unexpectedly terminate the process inside an operation that requires non-throwing elements. Failure before delegation SHALL NOT perform the affected underlying operation. The validation-layer factory SHALL retain its existing empty-result behavior on its own allocation failure. These CPU behaviors SHALL NOT establish native pipeline or shader support.

#### Scenario: Catch allocation failure through a public pipeline entry
- **WHEN** a valid pipeline request cannot allocate its CPU validation working storage
- **THEN** the caller can catch the allocation failure, no partial pipeline operation is delegated, and ordinary empty, single-layout and multi-layout requests preserve binding validation and delegated results when storage is available

#### Scenario: Validate a texture without an allocating capacity query
- **WHEN** a valid texture is checked while CPU allocations are unavailable
- **THEN** querying its representable subresource count requires no allocation, and any later allocation failure follows the applicable documented boundary instead of terminating during the capacity query
