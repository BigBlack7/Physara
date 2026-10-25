# Spec Delta

## Purpose

Define the first runnable Physara product lifecycle using the existing native host. Startup, event consumption, idle behavior and exit are observable and reproducible without implying that a renderer, editor or GPU submission path has been implemented.

## ADDED Requirements

### Requirement: Provide one real product entry
The project SHALL build one product executable that composes the implemented diagnostic owner and native host on the process main thread. Default startup SHALL create a visible native window. Hidden startup SHALL use the same verified host implementation. The executable SHALL NOT create placeholder renderer, editor or graphics-device owners or claim rendered output.

#### Scenario: Start the default product
- **WHEN** the product starts without options on a supported system
- **THEN** it enters its event lifecycle with a verified visible host and the existing owner-thread contract

#### Scenario: Start a hidden bounded run
- **WHEN** the product starts with hidden mode and a valid poll limit
- **THEN** it creates a real hidden host through the same implementation and exits successfully after the specified number of successful polls

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
The product SHALL consume each successful input publication before choosing whether to continue. It SHALL handle close intent and the configured poll limit before waiting. Iconified or zero-pixel windows SHALL continue event and close processing without allocating replacement graphics targets. During this bootstrap stage, both renderable and suspended idle windows SHALL use finite event waits rather than an unconditional busy loop or a device wait.

#### Scenario: Consume a close request
- **WHEN** the product receives published native close intent
- **THEN** it chooses orderly exit before another idle wait and does not rely on the callback to destroy the window

#### Scenario: Remain responsive while suspended
- **WHEN** the window is iconified or its framebuffer extent is zero and later recovers or receives close intent
- **THEN** event processing continues, restoration uses current window facts and no fake nonzero target or GPU submission is introduced

#### Scenario: Reach the configured poll limit
- **WHEN** the requested number of successful publications has been consumed
- **THEN** the product proceeds to shutdown without an extra wait or counting wait callbacks as additional polls

### Requirement: Exit in ownership order with meaningful results
Normal close and poll-limit completion SHALL return status 0 after successful shutdown. Diagnostic creation, host creation, event processing and explicit shutdown failures SHALL return status 1 with a bounded observable diagnostic. Host shutdown SHALL precede diagnostic-owner destruction on the owner thread. Cleanup or diagnostic-output failure SHALL NOT replace an earlier operation's failure or falsely imply that acquired state was ready.

#### Scenario: Complete normal shutdown
- **WHEN** a normal run exits and the same product is subsequently launched again
- **THEN** each process releases its host and callback targets before diagnostics and the later process can initialize normally

#### Scenario: Fail after startup has begun
- **WHEN** startup or a runtime operation fails, including failure while reporting or cleaning up the original error
- **THEN** the product preserves the original failure, releases every acquired owner in dependency order and returns status 1
