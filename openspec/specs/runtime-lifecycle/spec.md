# runtime-lifecycle Specification

## Purpose

Define how the Physara product composes diagnostics, the verified native host and the editor session on the main thread. The lifecycle specifies startup, input processing, CPU UI frame readiness, idle behavior and orderly shutdown.

## Requirements

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

### Requirement: Validate explicit startup options
The product SHALL support `--help`, `--hidden`, unsigned 32-bit logical `--width=N` and `--height=N`, and a positive unsigned 64-bit `--poll-limit=N`. Unknown, duplicate, malformed or out-of-range options SHALL exit with status 2 before native initialization. Help SHALL exit with status 0 without requiring native initialization. Zero dimensions SHALL reach host configuration validation and fail startup with status 1.

#### Scenario: Request help without a graphics environment
- **WHEN** the caller supplies valid options including help
- **THEN** usage is printed and status 0 is returned without creating the diagnostic owner or initializing the native library

#### Scenario: Reject invalid argument syntax
- **WHEN** an option is unknown, repeated, malformed, numerically out of range, or uses a zero poll limit
- **THEN** the product prints a bounded argument diagnostic and exits with status 2 before native initialization

#### Scenario: Reject invalid host dimensions
- **WHEN** a correctly parsed startup dimension is zero or exceeds the native dimension range
- **THEN** the normal host factory rejects configuration, the product reports its actual failure and exits with status 1 without a partially ready host

### Requirement: Separate event decisions from idle waiting
The product SHALL consume each successful input publication before choosing whether to continue. It SHALL handle close intent and the configured poll limit before another UI frame or idle wait. Iconified or zero-pixel windows SHALL continue event, input cleanup and close processing while suspending normal UI frame construction. The idle wait for both renderable and suspended windows SHALL be a finite platform event wait. On recovery the editor SHALL consume current observations.

#### Scenario: Consume a close request
- **WHEN** the product receives published native close intent
- **THEN** it chooses orderly exit before another UI frame or idle wait, and explicit shutdown releases the window

#### Scenario: Remain responsive while suspended
- **WHEN** the window is iconified or its framebuffer extent is zero and later recovers or receives close intent
- **THEN** event processing and interaction cleanup continue with finite waits, no replacement target is allocated or submitted, and a recovered UI frame uses current window facts

#### Scenario: Reach the configured poll limit
- **WHEN** the requested number of successful publications has been consumed
- **THEN** the product proceeds to shutdown without another UI frame or wait, and callbacks processed during earlier waits do not count as additional polls

### Requirement: Exit in ownership order with meaningful results
Normal close and poll-limit completion SHALL return status 0 after successful shutdown. Diagnostic creation, host or editor creation, runtime processing and explicit shutdown failures SHALL return status 1 with a bounded observable diagnostic. On the owner thread the editor SHALL release its input adapter and session state before host shutdown, which SHALL precede diagnostic-owner destruction. Cleanup or diagnostic-output failure SHALL NOT replace an earlier operation's failure or falsely imply that acquired state was ready.

#### Scenario: Complete normal shutdown
- **WHEN** a normal run exits and the same product is subsequently launched again
- **THEN** each process releases the editor callbacks before the host and diagnostics, and the later process can initialize normally

#### Scenario: Fail after startup has begun
- **WHEN** editor setup or another startup or runtime operation fails, including failure while reporting or cleaning up the original error
- **THEN** the product preserves the original failure, releases every acquired owner in dependency order and returns status 1
