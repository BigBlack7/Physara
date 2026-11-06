# OpenGL Device Availability Spec Delta

## MODIFIED Requirements

### Requirement: Reject unavailable operations without fictitious GPU work

The delivered device SHALL preserve the complete public interface and distinguish the documented resource/upload subset and baseline Shader, BindingLayout/BindingSet, InputLayout and GraphicsPipeline preparation from unavailable command execution, query and extension operations. The preparation subset SHALL be limited to the profile specified by opengl-shader-pipelines; successful creation or native program link SHALL NOT enable Draw, Dispatch, graphics-state execution or full rendering acceptance. Unsupported calls SHALL preserve output data where required and SHALL NOT create successful dummy objects, issue work or tickets, or wait for invented GPU work. Enabling either subset SHALL NOT imply general rendering support.

#### Scenario: Request a command list or submission
- **WHEN** a caller requests a list or submission mode outside the documented upload profile
- **THEN** the operation fails with no list, replay, issued work or positive submission result

#### Scenario: Request an excluded family with unused payload
- **WHEN** an unsupported extension operation is called with payload it does not need to inspect
- **THEN** it reports unsupported behavior before dereferencing the payload or forwarding native work

#### Scenario: Request an unavailable reference-only helper
- **WHEN** a caller requests the unsupported crash helper
- **THEN** the reason is reported and the documented logic-error failure occurs without returning a fake reference

#### Scenario: Separate supported preparation from unavailable drawing
- **WHEN** a baseline Shader, binding or graphics Pipeline is successfully prepared and the caller attempts a still-unimplemented graphics command
- **THEN** the prepared object is real while the command fails visibly without a Draw, replay or fictitious submission ticket
