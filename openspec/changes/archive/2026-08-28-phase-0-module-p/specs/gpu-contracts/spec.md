## Purpose

Defines the CPU-to-GPU contract check that must pass before layout or binding changes land, covering shared constants, structure sizes, bindings, enums, and shader declarations, and forbidding stale task numbers in contract sources.

## ADDED Requirements

### Requirement: Contract check is mandatory
Any change that edits CPU-to-GPU shared constants, structure layouts, buffer or texture bindings, or the matching shader declarations MUST be followed by a successful run of the project GPU contract checker. A failing check MUST be treated as a blocked change.

#### Scenario: Checker passes
- **WHEN** an operator runs the GPU contract checker against the current contract sources
- **THEN** the checker MUST exit successfully only if all hard checks pass

#### Scenario: Checker failure blocks the change
- **WHEN** the checker reports a hard mismatch between CPU and GPU contract sources
- **THEN** the change MUST NOT be signed off until the mismatch is resolved and the checker passes

### Requirement: Shared constants stay aligned
The checker SHALL verify that shared numeric limits used by both CPU upload code and shaders have the same values on both sides.

#### Scenario: Light and cascade limits match
- **WHEN** the checker compares the shared maximum light count and maximum shadow cascade count
- **THEN** the CPU values MUST equal the shader values

### Requirement: Structure layouts stay aligned
The checker SHALL verify that each paired CPU and shader contract structure occupies the same byte size under the project's packing rules. A size mismatch MUST fail the check.

#### Scenario: Paired structures match in size
- **WHEN** the checker compares each paired CPU and shader contract structure
- **THEN** the computed byte sizes MUST be equal

#### Scenario: Size mismatch fails
- **WHEN** a paired structure differs in computed byte size between CPU and shader sources
- **THEN** the checker MUST fail

### Requirement: Bindings and enums stay aligned
The checker SHALL verify that buffer bindings, texture bindings, light types, shading models, alpha modes, shadow filters, and object flags have the same numeric values in CPU headers and shader declarations. A value mismatch MUST fail the check.

#### Scenario: Binding values match
- **WHEN** the checker compares each named buffer and texture binding
- **THEN** the CPU enum value MUST equal the shader declaration value

#### Scenario: Enum value mismatch fails
- **WHEN** a shared enum or flag has different numeric values on the CPU and shader sides
- **THEN** the checker MUST fail

### Requirement: Soft hygiene warnings
The checker MAY emit warnings for overloaded bindings, unused binding declarations, or packing-risk member types. Warnings MUST NOT fail the check by themselves. Hard mismatches MUST still fail.

#### Scenario: Warning does not fail
- **WHEN** the checker finds an unused binding declaration or a packing-risk member but all hard checks pass
- **THEN** the checker MUST report the warning and still exit successfully

### Requirement: Stale task numbers are forbidden
Contract sources and the checker itself MUST NOT keep obsolete development-task numbers that no longer exist in the current plan. Comments MAY describe the check, but they MUST NOT point at retired task ids.

#### Scenario: Retired task id is removed
- **WHEN** a contract source or the checker still names a retired task id
- **THEN** that comment MUST be removed or rewritten before the module is signed off

#### Scenario: Current plan ids remain allowed
- **WHEN** a comment refers to the living module P contract-check work
- **THEN** the comment MAY remain
