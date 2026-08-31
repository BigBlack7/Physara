## Purpose

Defines the handmade performance baseline protocol: fixed run conditions, dual-machine slots, three render-path measurements, round overlay, and how panel, benchmark, capture, and visual feedback are used as later optimization evidence.

## ADDED Requirements

### Requirement: Fixed baseline run conditions
The project SHALL record performance numbers only under a declared, repeatable condition set. That set MUST include the default test scene `Assets/Scenes/default.scene.json`, a locked camera, a locked viewport resolution, locked render settings except the path under test, and a Release build. Debug or mixed-setting runs MUST NOT be treated as baseline or later-round evidence.

#### Scenario: Valid baseline capture
- **WHEN** an operator records a performance sample using the default test scene, a locked camera, a locked resolution, locked non-path settings, and a Release build
- **THEN** that sample MAY be stored as baseline or later-round evidence for the machine and render path under test

#### Scenario: Reject non-comparable sample
- **WHEN** a sample is taken from a Debug build, a different scene, a moved camera, a different resolution, or unlocked render settings
- **THEN** the sample MUST NOT replace the stored baseline or later-round numbers

### Requirement: Dual-machine slots
The baseline record SHALL provide two machine slots: machine B (low-power desktop) and machine A (high-power laptop). Completing the module SHALL require filled numbers for machine B. Machine A MAY remain empty and MUST NOT block sign-off.

#### Scenario: Machine B completes the module
- **WHEN** machine B has a complete three-path round-0 record and machine A is still empty
- **THEN** the baseline protocol is satisfied for this change

#### Scenario: Machine A remains reserved
- **WHEN** machine A numbers are not yet available
- **THEN** the record MUST keep an empty reserved slot for machine A and MUST NOT invent or copy machine B numbers into that slot

### Requirement: Three render paths
Each filled machine slot SHALL contain separate measurements for Forward, Forward+, and Deferred. A machine slot MUST NOT be marked complete until all three paths have been recorded under the same fixed conditions, changing only the render path.

#### Scenario: Incomplete path set
- **WHEN** only one or two of the three render paths have numbers for machine B
- **THEN** the machine B slot MUST remain incomplete

#### Scenario: Path-only variable
- **WHEN** the operator switches from Forward+ to Deferred for the next sample
- **THEN** scene, camera, resolution, build configuration, and other locked render settings MUST stay unchanged

### Requirement: Required measurement fields
Each path sample SHALL record hardware environment, CPU frame time, GPU frame time, per-pass CPU and GPU times that the current performance panel exposes, pipeline benchmark median and p95 when the built-in benchmark is used, and key submission counts needed to interpret those times. Missing fields MUST be marked unavailable rather than guessed.

#### Scenario: Panel and benchmark recorded
- **WHEN** the operator finishes a path sample with the performance panel visible and the built-in pipeline benchmark complete
- **THEN** the stored record MUST include hardware environment, CPU and GPU frame times, exposed per-pass times, and benchmark median plus p95

#### Scenario: Unknown field stays empty
- **WHEN** a desired timing or counter is not produced by the current panel, benchmark, or provided capture
- **THEN** the record MUST leave that field marked unavailable and MUST NOT fabricate a value

### Requirement: Evidence sources
Baseline and later-round numbers SHALL come from the in-editor performance panel, the built-in pipeline benchmark, and operator-supplied RenderDoc captures for the three paths. Visual correctness SHALL be judged only by the operator observing the scene and reporting the result. The project MUST NOT add golden images, screenshot regression, or automated performance scripts for this protocol.

#### Scenario: Capture arrives later
- **WHEN** panel and benchmark numbers are already stored and the operator later supplies a RenderDoc capture for a path
- **THEN** capture-derived timings MAY be added to that path record without requiring a new round number, provided run conditions are unchanged

#### Scenario: Visual feedback is operator-owned
- **WHEN** the operator reports that a path looks correct or names a visual defect after viewing the default scene
- **THEN** that report is the visual result for the round and MUST be stored as operator feedback, not as an automated image comparison

### Requirement: Round overlay
The first complete machine B record SHALL be marked round 0 and used as the comparison baseline. After a later performance change, new numbers for the same machine and path SHALL overwrite the previous numbers and MUST update only the round marker. Historical copies of old numbers are not required.

#### Scenario: Establish round 0
- **WHEN** machine B first has complete three-path measurements under the fixed conditions
- **THEN** those numbers MUST be stored as round 0

#### Scenario: Later optimization overwrites
- **WHEN** a later performance task remeasures the same machine and path under the same fixed conditions
- **THEN** the stored numbers MUST be replaced and the round marker MUST advance, without keeping a second historical table

### Requirement: Stage-one priority review
After machine B round 0 exists, the project SHALL review stage-one performance-related tasks against those numbers. The review MAY change documented priority or order and MUST record the evidence used. The review MUST NOT implement those stage-one tasks as part of establishing the baseline.

#### Scenario: Reorder from measured cost
- **WHEN** machine B round 0 shows a pass or submission path dominating frame time
- **THEN** the documented stage-one performance order MUST be updated or explicitly confirmed, with the measured reason recorded

#### Scenario: Review does not implement
- **WHEN** the priority review identifies a high-cost area owned by a later module
- **THEN** that work MUST remain outside this baseline change
