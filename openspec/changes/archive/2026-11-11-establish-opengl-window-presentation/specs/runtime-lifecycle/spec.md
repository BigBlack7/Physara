# Spec Delta

## MODIFIED Requirements

### Requirement: Provide one real product entry

The project SHALL build one product executable that composes the diagnostic owner, verified native host and editor session on the process main thread. Default startup SHALL create a verified visible native window. Hidden startup SHALL use the same host, editor and graphics assembly. Each renderer or graphics-device owner SHALL serve an implemented role. The editor SHALL consume a compatible UI Shader package for the running build configuration. The product SHALL distinguish CPU UI readiness, accepted graphics work and successful window presentation.

#### Scenario: Start the default product
- **WHEN** the product starts without options on a supported system and continues with a renderable window and an available target
- **THEN** the owner thread produces sealed CPU UI frames, composes them through RHI on initialized window-size color and presents the valid editor output

#### Scenario: Start a hidden bounded run
- **WHEN** the product starts with hidden mode and a valid poll limit
- **THEN** it uses the same verified host and editor rendering path for eligible iterations and exits successfully after the specified number of successful polls

#### Scenario: Fail when the required UI program cannot be initialized
- **WHEN** the configuration-matched UI Shader package is unavailable or incompatible during normal startup
- **THEN** the product reports a bounded startup failure, releases acquired owners in dependency order and exits with status 1
