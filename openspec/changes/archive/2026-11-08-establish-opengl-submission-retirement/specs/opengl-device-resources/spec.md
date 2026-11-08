# Spec Delta

## MODIFIED Requirements

### Requirement: Reject unavailable operations without fictitious GPU work

The delivered device SHALL preserve the complete public interface and distinguish documented resource/upload operations, baseline Shader, BindingLayout/BindingSet, InputLayout and GraphicsPipeline preparation, limited immediate graphics execution specified by opengl-immediate-draw, and Event/Timer queries specified by opengl-completion-queries from unavailable commands, readbacks and extensions. Preparation SHALL remain limited to opengl-shader-pipelines; successful creation or native program link alone SHALL NOT establish execution support or full rendering acceptance. Supported direct graphics commands and queries SHALL require their documented profile, ownership and state.

Unsupported calls SHALL preserve output data where required and SHALL NOT create successful dummy objects, issue work or tickets, or wait for invented GPU work. Enabling these subsets SHALL NOT imply Compute/UAV/indirect, public staging/map or asynchronous readback, other query extensions, multisample rendering, presentation or general rendering support beyond their documented profiles.

#### Scenario: Request a command list or submission
- **WHEN** a caller requests a list or submission mode outside the documented upload and immediate graphics profiles
- **THEN** the operation fails with no list, replay, issued work or positive submission result

#### Scenario: Request an excluded family with unused payload
- **WHEN** an unsupported extension operation is called with payload it does not need to inspect
- **THEN** it reports unsupported behavior before dereferencing the payload or forwarding native work

#### Scenario: Request an unavailable reference-only helper
- **WHEN** a caller requests the unsupported crash helper
- **THEN** the reason is reported and the documented logic-error failure occurs without returning a fake reference

#### Scenario: Separate supported preparation from unavailable drawing
- **WHEN** a baseline Shader, binding or graphics Pipeline is successfully prepared and the caller attempts a still-unimplemented graphics command such as indirect drawing
- **THEN** the prepared object is real while the command fails visibly without a Draw, replay or fictitious submission ticket

#### Scenario: Execute only the delivered direct graphics profile
- **WHEN** a caller uses the documented single-sample direct graphics operations with valid prepared objects and an owning open immediate list
- **THEN** the operations perform real native work under opengl-immediate-draw while other command families remain explicitly unavailable

#### Scenario: Use queries without inferring a readback implementation
- **WHEN** supported Event/Timer queries are used and a caller separately requests public staging/map or an excluded readback/query extension
- **THEN** the queries follow opengl-completion-queries while the excluded operation remains explicitly unsupported, with no fabricated pixels, row pitch or completed request

### Requirement: Report allocation separately from completed rendering

Delivery SHALL distinguish allocation, verified uploads, baseline program preparation, limited immediate graphics execution and verified query/in-flight retirement behavior from full completed rendering. It SHALL identify the tested profile, native environment and observed results, and keep deferred acceptance explicit. Allocation, link success, CPU validation or a query result alone SHALL NOT establish unobserved raster, full submission retirement or presentation behavior. Each evidence set SHALL cover only its implemented and actually observed data paths. A prerequisite exception SHALL stay limited to its explicitly approved work package and SHALL NOT close deferred acceptance.

#### Scenario: Report successful resource verification
- **WHEN** allocation, separately scoped upload/preparation, limited immediate graphics or query/retirement checks and their affected builds pass
- **THEN** completion identifies the capability actually verified and leaves excluded content/command modes, full-stage acceptance and presentation with their remaining work packages

#### Scenario: Encounter an unmet implementation prerequisite
- **WHEN** D01 still lacks its required cross-display acceptance or stage exit
- **THEN** only an explicitly approved exception for the current change permits its documented work, including this change's limited initial uploads, necessary immediate submission/lifetime/shutdown and minimum native readback, while D01 acceptance, full T05/D03, presentation and stage exits remain incomplete without inheriting earlier generic or T03 resource exceptions

#### Scenario: Separate the work-package and stage exits
- **WHEN** D03-T03 has satisfied its direct graphics, coordinate, data and minimum retirement-integration checks
- **THEN** D03-T04's full drawing-in-flight acceptance and D03-T05's stable image gate remain pending until their own requirements are satisfied, and no presentation or D03 stage-exit completion is inferred

#### Scenario: Complete the drawing-in-flight work package on its own evidence
- **WHEN** D03-T04 has verified real Draw submissions, later ordinary uploads, Event/Timer queries, in-flight references, required failure/exit paths and independent review
- **THEN** only D03-T04 may be marked complete on that evidence, while D03-T05's stable image gate, public asynchronous readback, presentation and the D03 stage exit keep their own acceptance requirements
