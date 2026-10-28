# Spec Delta

## Purpose

Define the first native OpenGL content-upload capability through the retained public RHI. Callers can release CPU input after accepted writes while real completion protects upload storage, referenced resources and device shutdown.

## ADDED Requirements

### Requirement: Record one owning immediate upload list
The upload profile SHALL accept only its documented graphics immediate-list mode under the owning current context. At most one list SHALL be open on that context. Commands SHALL require a valid open recording, and reuse SHALL require close and accepted submission. Unavailable modes and lifecycle violations SHALL fail without moving the context or consuming another recording.

#### Scenario: Use the host's validated upload list
- **WHEN** a ready host's owner creates a supported list and opens, writes, closes and submits it
- **THEN** the list uses the same native device through validation and accepted transitions occur once

#### Scenario: Reject an unavailable recording mode
- **WHEN** list creation requests deferred execution, a non-graphics queue or an unsupported external lifetime tracker
- **THEN** no usable list is returned and the unavailable dependency is not inspected or substituted

#### Scenario: Reject overlapping native recordings
- **WHEN** another list is opened while a raw or validated recording already owns the same context
- **THEN** the second open fails and the first recording retains its claim and data

#### Scenario: Reject the wrong native owner
- **WHEN** a recording or submission operation runs on a foreign thread or without the owning context current
- **THEN** it fails before the rejected native operation without migrating the context

#### Scenario: Reject an invalid list transition
- **WHEN** a caller writes before open, opens twice, closes twice or reopens a closed unsubmitted list
- **THEN** the operation is rejected without discarding already issued work or fabricating submission

### Requirement: Write checked ordinary buffer intervals
A supported ordinary buffer write SHALL update exactly the requested byte interval. Capacity, offset and native conversions SHALL be checked before source access or native work. A valid zero-length interval SHALL be harmless without reading its source; it SHALL NOT mean the remaining buffer. Writes outside the upload profile SHALL fail explicitly.

#### Scenario: Write a nonzero interval including a larger payload
- **WHEN** a caller writes a supported byte sequence at a valid offset, including a payload larger than 64 KiB
- **THEN** completed native readback matches that sequence and previously initialized bytes outside the interval remain unchanged

#### Scenario: Reject an invalid buffer interval
- **WHEN** size or offset exceeds capacity, wraps arithmetic or cannot be represented by the chosen native operation
- **THEN** the write fails before source access and issues no destination update

#### Scenario: Accept an empty interval at the boundary
- **WHEN** a supported buffer receives a zero-length write at an offset up to its capacity with a null source
- **THEN** no source is read, no upload storage is acquired and no native write is issued

### Requirement: Write the requested texture subresource
An accepted texture write SHALL target the complete requested mip of one layer or face, or the complete requested three-dimensional mip volume. Supported single-sample dimensions SHALL retain their public shape and array meaning. Unsupported content formats or sample modes SHALL be distinguishable from supported allocation and SHALL fail before the affected native write.

#### Scenario: Upload the supported texture shapes
- **WHEN** a caller uploads valid one-dimensional, two-dimensional, array, cube, cube-array or three-dimensional color content
- **THEN** completed readback identifies the requested mip and layer, face or volume with the expected dimensions

#### Scenario: Reject an invalid mip or layer
- **WHEN** a write selects an out-of-range mip, face or layer or a nonzero volume array slice
- **THEN** the whole write is rejected without clamping it to another subresource

#### Scenario: Request an excluded content mode
- **WHEN** a caller requests multisample, depth/stencil or another excluded format's content upload
- **THEN** it reports unsupported upload without reading the unused payload, changing allocation support or converting the format

#### Scenario: Keep untouched subresources distinct
- **WHEN** one mip or layer is written while other initialized subresources have different known contents
- **THEN** only the requested subresource changes and unwritten content is not declared initialized

### Requirement: Interpret pitches without reading padding as texels
Texture uploads SHALL validate the source footprint using checked row and depth strides before reading data. Padding SHALL remain outside the logical texel payload. Zero or irrelevant strides SHALL follow the documented dimension-dependent contract; invalid explicit strides SHALL fail without truncation or a native write.

#### Scenario: Copy padded rows and volume slices
- **WHEN** valid source rows or three-dimensional slices contain padding and asymmetric texel patterns
- **THEN** only the logical texels appear in completed destination readback in the declared order

#### Scenario: Reject a short or overflowing footprint
- **WHEN** row or depth pitch is too short or the last accessed byte cannot be represented safely
- **THEN** upload rejects the footprint before copying source bytes or modifying the destination

#### Scenario: Apply only relevant strides
- **WHEN** a one-row image uses zero row pitch or a single-depth image supplies an otherwise unused depth pitch
- **THEN** the documented row-size default or irrelevant-stride behavior is preserved without an unnecessary overflow calculation

### Requirement: Preserve uploaded encoding and row order
Upload SHALL retain the documented component type, stored color encoding and logical row/slice order of supported color formats. It SHALL NOT introduce gamma conversion, image flipping, sampler changes or component remapping. Storage swizzle and sampling interpretation SHALL remain independent of the uploaded texel bytes.

#### Scenario: Upload supported integer and floating color data
- **WHEN** callers upload known values in each permitted color format
- **THEN** native readback in the corresponding representation matches those values without a type or channel substitution

#### Scenario: Keep encoded sRGB bytes separate from sampling
- **WHEN** the same known encoded bytes are uploaded to permitted linear and sRGB storage
- **THEN** their stored values retain the supplied bytes and upload performs no additional color-space transform

#### Scenario: Retain asymmetric image direction
- **WHEN** source rows, slices or cube faces carry distinct markers and the texture has a nondefault sampling swizzle
- **THEN** readback preserves their upload order while the texture's swizzle and sampler state remain unchanged

### Requirement: Consume borrowed CPU data before write returns
An accepted write SHALL finish consuming the caller's source before returning, either by completing the native client-data consumption or copying into owned upload storage. No later close, submission or GPU completion SHALL require the original pointer. This CPU boundary SHALL NOT imply that the GPU transfer has completed.

#### Scenario: Destroy or overwrite the source immediately
- **WHEN** a caller overwrites and releases source storage immediately after accepted buffer or texture writes return and only then closes and submits
- **THEN** completed readback contains the original submitted values without later source access

#### Scenario: Keep GPU ownership after CPU input is gone
- **WHEN** CPU input has been released while an accepted staged transfer remains pending or in flight
- **THEN** its upload allocation and referenced destination remain retained until their actual retirement condition is satisfied

### Requirement: Bound pending upload storage and work
The initial profile SHALL declare finite limits for owned upload storage, outstanding recordings and per-recording writes. Limit exhaustion SHALL be observable and SHALL NOT silently grow storage, overwrite a pending range or perform an implicit blocking wait. Completed retirement SHALL release the associated budget; idle retention SHALL remain bounded by the documented policy.

#### Scenario: Exhaust the upload byte budget
- **WHEN** a new write would exceed the retained upload-storage limit
- **THEN** it is rejected before its native write with requested, retained and limit information available and prior work still retained

#### Scenario: Exhaust recording or write capacity
- **WHEN** outstanding recordings or accepted writes reach their declared limit
- **THEN** another recording or write fails without losing an earlier packet or creating an unbounded queue

#### Scenario: Recover budget after completion
- **WHEN** a completed packet is collected and an equivalent allocation is requested again
- **THEN** its previous upload storage has been retired and the released budget is available without a stale lease

#### Scenario: Preserve a pending allocation
- **WHEN** a recording is closed or CPU handles are released before completion has been established
- **THEN** its upload allocation remains unavailable for destructive reuse despite elapsed CPU frames or available caller memory

### Requirement: Keep transfer-state publication separate from recording
Supported upload-state operations SHALL use one consistent tracking authority for the actual resource and subresource. A known state declaration SHALL NOT masquerade as a transition. Invalid tracking SHALL prevent a valid submission; closing, abandoning or merely completing native work SHALL NOT publish accepted-submission metadata. Tracking metadata SHALL NOT certify untouched pixels or GPU completion.

#### Scenario: Infer a supported transfer use
- **WHEN** a recording has a valid known prior state and automatic upload tracking is enabled
- **THEN** the write establishes and consumes the required transfer dependency before native work

#### Scenario: Use explicit transfer tracking
- **WHEN** automatic inference is disabled and the caller declares, requests and commits valid upload states
- **THEN** the same tracking authority validates the transfer without a parallel automatic dependency

#### Scenario: Reject invalid tracking
- **WHEN** prior state is unknown, a requested range is invalid or the tracker has failed
- **THEN** no affected native write or successful submission is issued and external tracking metadata remains unpublished

#### Scenario: Close or discard without accepted submission
- **WHEN** a recording is closed or discarded after valid upload work but before a submission is accepted
- **THEN** no initialized or permanent metadata is published merely because the recording ended or its cleanup later completed

#### Scenario: Accept submission metadata without inventing content
- **WHEN** a valid upload recording is successfully accepted for submission
- **THEN** its documented tracking metadata is published at that stage without declaring unrelated content initialized or the GPU complete

### Requirement: Submit closed upload batches without replay
Each submission boundary SHALL preflight the complete batch before consuming lifecycle state owned by that boundary. A positive identity SHALL require a real fence and successful advancement of issued work. Execution SHALL NOT replay immediate writes. Validation SHALL preserve delegated failure without restoring its consumed wrapper states. Invalid, foreign, duplicated or consumed lists SHALL fail; an empty batch SHALL return zero without work.

#### Scenario: Submit a valid closed batch
- **WHEN** a supported closed batch passes preflight and native fence creation and flush succeed
- **THEN** it receives one valid increasing submission identity and writes are not issued a second time

#### Scenario: Reject a later invalid batch element
- **WHEN** an early closed list is valid but a later batch element fails preflight at the rejecting submission boundary
- **THEN** that boundary returns zero before consuming the earlier lifecycle state it owns or publishing successful resource tracking metadata

#### Scenario: Reject foreign or duplicated lists
- **WHEN** a raw or validated batch contains another device's list, a wrong live implementation or the same list twice
- **THEN** the complete batch fails before a tracking fence is created and the rejecting boundary retains the lifecycle states it owns

#### Scenario: Preserve a delegated submission failure
- **WHEN** a validated batch passes wrapper preflight but the underlying device returns zero
- **THEN** zero is preserved, consumed wrapper states are not restored for replay, and any already issued native work retains its ownership until safe retirement

#### Scenario: Reject repeated execution
- **WHEN** a caller attempts to execute an already consumed recording again
- **THEN** it receives no positive result and no upload is replayed

#### Scenario: Submit an empty batch
- **WHEN** a caller submits a supported empty batch
- **THEN** the result is zero without fence creation, work replay or a manufactured completion

### Requirement: Reclaim only proven completed work
Nonblocking collection SHALL distinguish Complete, Pending and Failed using actual native results. Only Complete SHALL authorize completion advancement and associated upload/reference release. Failed SHALL stop new work and retain unproven ownership. An explicit idle wait SHALL report actual completion or a bounded failure/timeout without becoming an implicit upload wait.

#### Scenario: Poll pending work
- **WHEN** a valid tracking fence reports that work remains pending
- **THEN** collection returns without waiting, advancing completion or releasing that packet's references and storage

#### Scenario: Collect completed work once
- **WHEN** a valid tracking fence reports completion and collection runs repeatedly
- **THEN** completion advances legitimately and each associated reference, fence and upload allocation is retired exactly once

#### Scenario: Fail a completion query
- **WHEN** a completion query fails rather than reporting pending
- **THEN** new work stops, unproven references remain retained and no failed identity is reported complete

#### Scenario: Wait explicitly with a deadline
- **WHEN** a caller explicitly waits for issued device work
- **THEN** success requires real completion and a timeout returns failure while retaining unproven work without treating timeout as device loss

### Requirement: Retire abandoned immediate work safely
Abandoning a list SHALL distinguish work that never reached the native API from an irreversible issued prefix. Unsubmitted issued work SHALL retain its resources and upload storage until real retirement or controlled native-environment exit. Final list release SHALL perform no foreign-thread GL work or new retirement allocation and SHALL NOT publish successful submission metadata.

#### Scenario: Discard a recording with no issued work
- **WHEN** an unused recording is released before any native upload
- **THEN** its CPU recording claim and budget are released without submission or completion being invented

#### Scenario: Discard after an upload was issued
- **WHEN** a list is released after native upload but before accepted submission
- **THEN** that issued prefix remains retained for owner-context retirement and is not treated as cancelled GPU work or a successful public submission

#### Scenario: Release a list on a worker
- **WHEN** a worker performs the final list release after its user has stopped
- **THEN** no GL call or retirement allocation occurs there and the owning device can subsequently retire its issued work

#### Scenario: Preserve an issued prefix after failure
- **WHEN** a later native upload or tracking failure makes a recording invalid after an earlier operation was issued
- **THEN** the recording cannot obtain a normal success result and its entire unproven prefix remains protected

### Requirement: Preserve upload failures transactionally
Each write SHALL prepare its required CPU ownership, bookkeeping and upload capacity before issuing that write. Invalid inputs and CPU preparation failure SHALL leave that write unapplied while preserving previously accepted work. Native failures SHALL preserve their operation and first error, release safely unused preparations and retain any possibly issued work. Cleanup diagnostics SHALL NOT replace the primary failure.

#### Scenario: Fail CPU preparation before a write
- **WHEN** reference, tracker or upload bookkeeping preparation fails before the affected native command
- **THEN** that write performs no destination update and earlier accepted work is not discarded or replayed

#### Scenario: Fail upload allocation or mapping
- **WHEN** native upload storage creation or mapping fails after a real object was acquired
- **THEN** no partial successful upload is reported and cleanup preserves the original failure without leaking an unused native allocation

#### Scenario: Fail a native write
- **WHEN** a native operation fails after it may have consumed upload storage
- **THEN** continued use stops as required and potentially referenced storage remains protected rather than returning to an available pool

#### Scenario: Fail fence creation or advancement
- **WHEN** tracking-fence creation or command flushing fails after immediate work has been issued
- **THEN** submission returns zero, the work remains retained and neither a positive ticket nor completion is fabricated

### Requirement: Distinguish internal retention from external ownership
Device-owned upload retention SHALL protect destination wrappers without being mistaken for an external owner at shutdown. Extra caller resources, command lists or device references SHALL still prevent destructive shutdown. Ownership checks SHALL use actual device/resource relationships and accounted internal holds, without inferring identity from a native name or replacing ownership with a global address registry.

#### Scenario: Release all caller resource handles in flight
- **WHEN** callers release their buffer and texture handles while accepted work remains in flight
- **THEN** the device retains their wrappers until proven retirement and internal ownership alone does not force a ResourceInUse rejection

#### Scenario: Retain a resource across several packets
- **WHEN** the same native resource is retained by several upload packets and optionally by an extra caller handle
- **THEN** completion releases only the relevant internal holds and the extra caller handle remains distinguishable until it is released

#### Scenario: Keep a caller list alive
- **WHEN** a caller still owns an upload list while requesting host shutdown
- **THEN** it remains an external owner even if its packet has been submitted, and shutdown is retryable without invalidating the list

### Requirement: Drain issued work before native-environment release
Shutdown SHALL first reject genuine external ownership without disturbing an active device. Once draining starts, new work SHALL stop and only proven-complete work or the declared unavailable-context exit SHALL release ownership. Pending drain SHALL retain the native environment for retry. Unfenced or failed-query work on a usable context SHALL be conservatively gathered only at controlled exit; confirmed unusability SHALL stop further GL calls without false completion.

#### Scenario: Close with internal submitted work only
- **WHEN** callers have stopped and released their owners while the device retains submitted uploads
- **THEN** shutdown gathers actual completion before releasing upload storage, wrappers and the context

#### Scenario: Retry an incomplete drain
- **WHEN** a usable context's shutdown wait reaches its declared deadline while work is still pending
- **THEN** shutdown reports a retryable incomplete drain, retains necessary owners and native state, and rejects new work until a later shutdown can finish

#### Scenario: Exit with work lacking a reliable fence
- **WHEN** controlled shutdown must retire issued work without a usable tracking result while its context remains usable
- **THEN** it uses the documented exit-only native gathering path before releasing ownership and does not mint a public submission identity for that work

#### Scenario: Exit after the context becomes unavailable
- **WHEN** the context is confirmed unusable or final native gathering cannot establish a usable environment
- **THEN** the declared unavailable-context path releases CPU bookkeeping, stops GL calls and requires final context release without reporting the work complete
