# Native window host delta

## ADDED Requirements

### Requirement: Confirm live window title and swap interval requests

The live host SHALL accept separate title and VSync requests for its current window identity on the owner thread at a frame boundary outside callbacks and graphics recording. Title edits SHALL accept well-formed UTF-8 up to 255 bytes without embedded NUL. VSync requests SHALL select swap interval 0 or 1 on the owning drawable. The host SHALL provide distinct requested and last confirmed effective values for each field, with an unavailable effective value before native confirmation. It SHALL confirm a title from the actual window caption and a swap interval from supported native control on the current context and drawable. The confirmed interval describes the native setting. PresentWait and GPU completion retain their own observations.

An eligible request awaiting its application boundary SHALL remain pending. A rejected value or an unconfirmed native change SHALL expose a bounded reason and preserve the last confirmed effective value. Invalid input SHALL be rejected before applying it. Failure confined to one setting SHALL leave the other setting independently applicable and allow the editor to continue on a usable host. Startup environment snapshots SHALL retain their creation-time meaning.

#### Scenario: Apply a committed window title
- **WHEN** the editor submits a valid title for the live window at the request boundary
- **THEN** the actual caption and effective title agree with the request after confirmation, and editor layout and View edits keep their own owners

#### Scenario: Apply a supported swap interval
- **WHEN** the owner requests a VSync change on a live context and drawable that support native swap control
- **THEN** successful confirmation reports the requested interval as effective for subsequent presentation, independently of observed swap waiting and GPU completion

#### Scenario: Retain effective settings after a rejected request
- **WHEN** the title is invalid, native control is unsupported, or native confirmation fails
- **THEN** the affected field reports the reason and retains its last confirmed effective value, while an independent valid request can still succeed on the usable host

#### Scenario: Reject a request outside its owner boundary
- **WHEN** a settings request targets a stopped or replaced window identity or is called outside the owner thread
- **THEN** the host rejects the request before native mutation and keeps the live window and its settings intact
