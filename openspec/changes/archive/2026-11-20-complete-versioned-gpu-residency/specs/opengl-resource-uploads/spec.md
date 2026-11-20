# Spec Delta

## MODIFIED Requirements

### Requirement: Bound pending upload storage and work

The OpenGL upload profile SHALL declare finite limits for mapped storage, outstanding recordings and per-recording writes. Large supported buffer writes and supported texture writes SHALL reuse eligible persistent/coherent storage for native copy or PBO upload. Admission SHALL cover the same sequence, alignment, chunk and capacity rules as allocation. The budget SHALL include idle retained storage and storage leased by recordings and submissions. Limit exhaustion SHALL be observable and SHALL NOT silently grow storage, overwrite a leased range or perform an implicit blocking wait. Completed retirement SHALL return eligible storage to bounded reuse or release its capacity for other allocations.

#### Scenario: Exhaust the upload byte budget
- **WHEN** a new write would exceed the retained upload-storage limit
- **THEN** it is rejected before its native write with requested, retained and limit information available and prior work still retained

#### Scenario: Exhaust recording or write capacity
- **WHEN** outstanding recordings or accepted writes reach their declared limit
- **THEN** another recording or write fails without losing an earlier packet or creating an unbounded queue

#### Scenario: Recover budget after completion
- **WHEN** a completed packet is collected and an equivalent allocation is requested again
- **THEN** the previous packet's lease is retired and its compatible safe storage is reusable without allocating a new chunk; incompatible idle capacity can be released under the same budget

#### Scenario: Preserve a pending allocation
- **WHEN** a recording is closed or CPU handles are released before completion has been established
- **THEN** its upload allocation remains unavailable for destructive reuse despite elapsed CPU frames or available caller memory

## ADDED Requirements

### Requirement: Version graphics dynamic constant writes until their real retirement

An accepted volatile constant buffer write SHALL copy a supported nonempty prefix at logical offset zero into an available aligned version owned by the current recording. The version SHALL record the bytes written, and other bytes SHALL remain unavailable for consumption by that write. An accepted graphics push constant write SHALL consume exactly the size declared by the active compatible layout and copy the bytes into independently versioned UBO storage. Both writes SHALL consume the borrowed source during the call. Allocation and validation SHALL apply before reassigning any affected version.

A written version selected by immediate Draw SHALL remain valid through that use and real retirement. Pending, closed and submitted versions SHALL remain distinct from available versions; a submitted version becomes reusable only after completion of its work. An invalid request or exhausted capacity SHALL report the operation and reason without overwriting referenced data or publishing a new version. Already issued work SHALL remain protected on invalid submission, abandonment, collection failure or shutdown under the existing retirement contract.

#### Scenario: Exhaust and reuse a small volatile version ring
- **WHEN** each version of a small valid volatile constant buffer has been consumed by a recording or a not-yet-retired submission, and another write is attempted before and after matched completion is observed
- **THEN** the first request reports exhaustion without changing prior draw data; after retirement the next accepted write can use the released version, including the first slot after wrap

#### Scenario: Keep dynamic operands of an issued prefix
- **WHEN** a recording has issued a draw with volatile and push data before it encounters a failure or is abandoned
- **THEN** the associated ranges remain unavailable for reuse until their work is proven complete or legally drained on exit, even when the list and the original input storage have been released
