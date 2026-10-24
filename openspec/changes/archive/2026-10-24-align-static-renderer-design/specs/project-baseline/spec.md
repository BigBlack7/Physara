# Spec Delta

## MODIFIED Requirements

### Requirement: Separate model assets from editor scenes
The architecture SHALL scope Physara to static rendering and editing, with read-only glTF/GLB models and one JSON per scene. It SHALL define Assets references, stable entity identity, hierarchy, object/render parameters, instance material/texture overrides, ownership, load/save validation and failures without embedding asset bytes or GPU objects. It SHALL exclude animation playback, skinning and morphing, and describe unsupported behavior and resource provenance honestly.

#### Scenario: Review persistence design
- **WHEN** a maintainer reads the Editor, Asset and Scene design
- **THEN** model import and editor scene persistence have consistent responsibilities, file locations and formats

#### Scenario: Review continued scene editing
- **WHEN** a maintainer follows the documented open, edit and save flow
- **THEN** the scene JSON restores referenced assets and editable scene parameters, and instance edits are saved without rewriting glTF/GLB sources

#### Scenario: Review static rendering scope
- **WHEN** a maintainer compares the architecture and development plan
- **THEN** both describe static geometry with editor and camera changes, and neither promises animation playback, skinning or morph implementation
