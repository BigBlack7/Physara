# Spec Delta

## Purpose

Define live window observations, ordered input publication and bounded platform requests on the verified native host. Consumers can interpret input and close intent without owning native callbacks, guessing coordinate units or retaining expired event storage.

## ADDED Requirements

### Requirement: Observe independent live window facts
The host SHALL publish observed logical client dimensions, framebuffer dimensions in pixels, content scale, focus, iconification and close intent. Dimensions and scale SHALL remain distinct quantities. A checked metrics revision SHALL identify observed dimension or scale changes. A window SHALL be renderable only when it is not iconified and both framebuffer dimensions are positive.

#### Scenario: Obtain initial live observations
- **WHEN** the first input publication succeeds
- **THEN** window facts originate from the native window rather than being copied permanently from its creation request

#### Scenario: Observe independent metric changes
- **WHEN** logical size, pixel size or content scale changes independently, including a sequence of rapid changes
- **THEN** publication preserves their separate units and latest observations, advances the revision for actual metric changes and does not advance it for identical repeated observations

#### Scenario: Suspend and restore renderability
- **WHEN** the window becomes iconified or either framebuffer dimension becomes zero and is later restored
- **THEN** the published state becomes non-renderable and later becomes renderable from current facts without inventing a nonzero replacement size

### Requirement: Publish each input interval once in order
Each successful event poll SHALL publish one monotonically numbered batch containing the ordered events accumulated since the previous publication and a value snapshot of current input and window state. Polling SHALL retain events received before the poll call. Published values SHALL be independent of subsequent accumulation; event storage SHALL be read-only and borrowed until the next accepted owner-thread poll begins or host shutdown begins.

#### Scenario: Retain callbacks between polls
- **WHEN** callbacks arrive during a wait or another native operation before the next poll
- **THEN** the next successful publication includes them once in arrival order instead of clearing them at poll entry

#### Scenario: Retain a published batch during accumulation
- **WHEN** a consumer retains a published batch while the host waits or applies valid platform requests
- **THEN** that batch remains unchanged until its declared borrow expires, and retaining events beyond that point requires an owned copy

#### Scenario: Publish an interval without new events
- **WHEN** a later poll receives no new events
- **THEN** it publishes a new sequence with no repeated events, clears prior interval edges and deltas, and preserves held state

### Requirement: Preserve key transitions and committed text meaning
Known keys and mouse buttons SHALL distinguish held, pressed and released state while preserving transition order. Key repeat SHALL remain a distinct event rather than another first press. Unknown logical keys SHALL NOT index known-key storage. Committed text SHALL preserve ordered Unicode scalar values independently of physical key events and SHALL NOT claim an IME composition interface.

#### Scenario: Press repeat and release within one interval
- **WHEN** a known key or button is pressed and released before publication, with key repeat events where applicable
- **THEN** pressed and released can both be true, final held state is false, event order remains available and repeat does not create an additional first-press edge

#### Scenario: Receive an unknown logical key
- **WHEN** a native key cannot be mapped to a known logical key
- **THEN** its event and opaque scan information can be observed without indexing outside known-key state or suppressing later text events

#### Scenario: Receive committed Unicode text
- **WHEN** several committed characters arrive, including non-ASCII and non-BMP scalar values
- **THEN** publication preserves every scalar value and its order without deriving text from key codes

### Requirement: Preserve pointer and scroll units
Pointer positions and motion SHALL use the window's logical coordinate space with sufficient precision for fractional values. Scroll SHALL retain its signed fractional native scroll offsets and SHALL NOT be reinterpreted as framebuffer pixels or multiplied by content scale. Relative cursor mode SHALL remain distinct from a claim of raw device motion.

#### Scenario: Observe fractional pointer movement
- **WHEN** the pointer moves by fractional logical coordinates after a valid motion baseline exists
- **THEN** the snapshot and ordered events retain that precision and a consistent logical motion delta

#### Scenario: Observe fractional scrolling under scaling
- **WHEN** fractional horizontal or vertical scroll input arrives on a scaled window
- **THEN** the ordered offsets and accumulated offsets retain their sign and value without an additional DPI conversion

### Requirement: Reset held and relative motion state at focus boundaries
Focus loss SHALL release held keys and buttons in the published state, clear accumulated pointer motion and arrange release of relative cursor mode at a safe native boundary. Focus regain SHALL NOT automatically resume capture. The first pointer sample after focus regain or an effective cursor-mode change SHALL establish a new baseline with zero motion. Native release events following focus loss SHALL NOT be duplicated by fabricated physical events.

#### Scenario: Lose focus while input is held
- **WHEN** focus is lost while keys or mouse buttons are held and relative cursor mode is active
- **THEN** held state is cleared, release edges remain observable, pointer motion is zeroed and capture is released outside the callback

#### Scenario: Regain focus
- **WHEN** focus returns and pointer input resumes
- **THEN** capture remains released and the first sample cannot create a jump from the previous baseline

#### Scenario: Change the effective cursor mode
- **WHEN** a supported cursor-mode request succeeds and subsequent pointer samples arrive
- **THEN** the mode transition resets motion and its first sample establishes a zero-delta baseline

### Requirement: Publish close intent without destroying the window
A native close event SHALL publish close intent without destroying the window or choosing application policy. The caller SHALL be able to clear a pending close request at the platform-request boundary. Previously published values SHALL retain the close state they originally reported.

#### Scenario: Receive native close intent
- **WHEN** the native window receives a close request
- **THEN** the request becomes observable while the host remains alive until its caller explicitly shuts it down

#### Scenario: Cancel pending close intent
- **WHEN** the caller clears close intent through a successful platform request and no newer close event arrives before publication
- **THEN** a later publication reports the cleared state, the older publication remains unchanged and a subsequent native close can be observed again

### Requirement: Apply requests through an explicit safe boundary
The host SHALL accept borrowed ordered requests for normal or relative cursor mode and close-flag changes outside native callbacks. It SHALL reject invalid request values and an initially unfocused capture request before applying the batch. Native failure after a successful prefix SHALL identify the failing request index and SHALL NOT claim atomic rollback or successful completion of the remaining suffix. Effective observations SHALL remain distinct from requested values.

#### Scenario: Apply a supported request batch
- **WHEN** the caller applies a valid request batch at the request boundary
- **THEN** requests are handled in order, native callbacks accumulate for later publication and existing published storage remains unchanged

#### Scenario: Reject a batch during preflight
- **WHEN** any request has an invalid value or requests relative capture while the window is already unfocused
- **THEN** the batch is rejected without changing native state, including changes from an otherwise valid prefix

#### Scenario: Fail after a request prefix
- **WHEN** a native request fails after earlier requests were applied, or focus loss prevents a requested capture from remaining effective
- **THEN** the result identifies the affected index, preserves the primary failure or rejection, and does not hide earlier effects or overwrite the result with cleanup failure

### Requirement: Preserve callback ownership and integration order
Input callbacks SHALL use the host's stable native user-data association for their valid lifetime. A later callback adapter SHALL be able to chain to them without replacing that association or duplicating input delivery. The caller SHALL stop such an adapter before shutting down the host; host shutdown SHALL disconnect its input targets before freeing their storage.

#### Scenario: Chain another callback adapter
- **WHEN** an adapter installs after the platform callbacks and forwards an input event to them
- **THEN** both consumers receive the event once and the host's native user-data association remains intact

#### Scenario: Stop integration before host release
- **WHEN** the adapter restores the platform callbacks and the host then shuts down or rolls back
- **THEN** native input targets are disconnected before their storage is released and subsequent host creation has no reference to the old state

### Requirement: Bound input retention and fail incomplete batches visibly
Event accumulation SHALL have a declared finite capacity and SHALL preserve transitions and committed text within that capacity. Overflow, invalid scalar or numeric input, and sequence or revision exhaustion SHALL produce distinguishable bounded failures without publishing a partial batch as successful. Callbacks SHALL NOT leak exceptions. Failure handling SHALL clear held and motion state, attempt safe capture release and allow ordered shutdown and fresh host creation.

#### Scenario: Exceed the event budget
- **WHEN** accumulation exceeds the declared event capacity
- **THEN** the failed batch is reported explicitly, no valid partial batch is returned, input state is reset and further publication requires host recreation

#### Scenario: Receive invalid input data
- **WHEN** committed text is not a Unicode scalar value or native numeric observations cannot satisfy their declared ranges
- **THEN** the host reports an input failure without exposing invalid indices, non-finite motion or fabricated valid window facts

#### Scenario: Exhaust a publication counter
- **WHEN** advancing the publication sequence or metrics revision would wrap
- **THEN** publication fails visibly rather than reissuing an older value and the host remains able to shut down

### Requirement: Process idle events without premature publication
Event waiting SHALL accept only finite positive timeout values expressed in seconds, process native events and leave them pending for the next event publication. Waiting SHALL NOT advance the published sequence or invalidate the currently borrowed batch. A native timeout request SHALL NOT be presented as a real-time completion guarantee.

#### Scenario: Receive input while waiting
- **WHEN** the owner waits with a valid timeout and an input event or timeout occurs
- **THEN** waiting returns without publishing a batch, and the next poll publishes any accumulated input once

#### Scenario: Reject an invalid wait duration
- **WHEN** the timeout is zero, negative, NaN or infinite
- **THEN** the call returns a configuration failure before native event processing and leaves existing input state and borrowed storage unchanged

### Requirement: Enforce operation preconditions and preserve failures
Event polling, waiting and request application SHALL obey the existing host ownership contract, reject stopped-host and reentrant operations before native work, and expose bounded contextual failures. Input setup SHALL be complete before host creation succeeds. Input setup or processing failure SHALL preserve its primary context while cleanup remains independently observable and normal shutdown stays available.

#### Scenario: Reject a wrong-thread or stopped-host operation
- **WHEN** a non-owner thread calls an input operation, or the owner calls one after shutdown
- **THEN** the call fails before native work or mutation of another valid publication

#### Scenario: Reject a reentrant operation
- **WHEN** a callback tries to poll, wait, apply requests or explicitly shut down the host during native event processing
- **THEN** the nested operation is rejected without destroying the active callback target or changing its process claim

#### Scenario: Fail during input setup
- **WHEN** input initialization fails after the native context or some callbacks have been acquired
- **THEN** creation returns no ready host and the existing rollback protocol disconnects acquired callbacks and permits later creation

#### Scenario: Fail during native event processing
- **WHEN** native polling, waiting or observation fails, including an additional error during capture release
- **THEN** the primary operation failure remains identifiable, no successful incomplete batch is returned and orderly shutdown remains possible
