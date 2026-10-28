# Spec Delta

## MODIFIED Requirements

### Requirement: Reject unavailable operations without fictitious GPU work
The delivered device SHALL preserve the complete public interface and distinguish the documented upload-list, content and submission subset from unavailable shader, binding, draw, query and extension operations. Unsupported calls SHALL preserve output data where required and SHALL NOT create successful dummy objects, issue work or tickets, or wait for invented GPU work. Enabling the upload subset SHALL NOT imply general rendering support.

#### Scenario: Request a command list or submission
- **WHEN** a caller requests a list or submission mode outside the documented upload profile
- **THEN** the operation fails with no list, replay, issued work or positive submission result

#### Scenario: Request an excluded family with unused payload
- **WHEN** an unsupported extension operation is called with payload it does not need to inspect
- **THEN** it reports unsupported behavior before dereferencing the payload or forwarding native work

#### Scenario: Request an unavailable reference-only helper
- **WHEN** a caller requests the unsupported crash helper
- **THEN** the reason is reported and the documented logic-error failure occurs without returning a fake reference

### Requirement: Report allocation separately from completed rendering
Delivery SHALL distinguish allocation, verified uploads and completed rendering, identify its tested profile, native environment and observed results, and keep deferred acceptance explicit. Allocation or CPU validation alone SHALL NOT establish upload, raster, synchronization or presentation behavior. Upload evidence SHALL cover only implemented data paths. A prerequisite exception SHALL stay limited to its explicitly approved work package and SHALL NOT close deferred acceptance.

#### Scenario: Report successful resource verification
- **WHEN** allocation checks or the separately scoped native upload checks and affected builds pass
- **THEN** completion identifies the capability actually verified and leaves excluded content modes, full-stage acceptance, drawing and presentation with their remaining work packages

#### Scenario: Encounter an unmet implementation prerequisite
- **WHEN** D01 still lacks its required cross-display acceptance or stage exit
- **THEN** only an explicitly approved exception for the current change permits its documented work, including this change's limited initial uploads, necessary immediate submission/lifetime/shutdown and minimum native readback, while D01 acceptance, full T05/D03, presentation and stage exits remain incomplete without inheriting earlier generic or T03 resource exceptions
