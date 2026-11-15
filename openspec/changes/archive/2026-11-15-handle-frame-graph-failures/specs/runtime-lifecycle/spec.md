# Spec Delta

## MODIFIED Requirements

### Requirement: Exit in ownership order with meaningful results

Normal close and poll-limit completion SHALL return status 0 after successful shutdown. Diagnostic creation, host or editor creation, required base UI initialization, unrecoverable runtime processing and explicit shutdown failures SHALL return status 1 with a bounded observable diagnostic. A recoverable frame failure SHALL be reported and resolved through the frame graph fallback or cancellation contract while event processing and retirement continue; that handled failure alone SHALL NOT force a process failure result. A Fatal graphics diagnostic or loss of reliable submission or completion ownership SHALL stop new rendering and enter controlled shutdown.

On the owner thread the editor SHALL release its input adapter and session state before host shutdown, which SHALL precede diagnostic-owner destruction. Cleanup or diagnostic-output failure SHALL NOT replace an earlier operation's failure or falsely imply that acquired state was ready. Fatal severity SHALL NOT be interpreted as proof of native device loss; shutdown SHALL follow the backend's actual health and completion facts.

#### Scenario: Complete normal shutdown

- **WHEN** a normal run exits and the same product is subsequently launched again
- **THEN** each process releases the editor callbacks before the host and diagnostics, and the later process can initialize normally

#### Scenario: Fail after startup has begun

- **WHEN** editor setup or another startup or unrecoverable runtime operation fails, including failure while reporting or cleaning up the original error
- **THEN** the product preserves the original failure, releases every acquired owner in dependency order and returns status 1

#### Scenario: Continue after a handled frame failure

- **WHEN** a running editor handles a graph failure through a successful safe UI frame or safe cancellation, later renders a valid normal frame and then receives normal close intent
- **THEN** input and retirement remain responsive across the failed iteration, no failed output is published, and successful shutdown returns status 0

#### Scenario: Preserve startup failure for a required font

- **WHEN** the required base font cannot be prepared or its initial upload cannot be recorded and accepted
- **THEN** fallback does not hide the startup failure or report the editor ready, outstanding work is retained for controlled shutdown, and the product returns status 1

#### Scenario: Stop after a fatal preparation result

- **WHEN** frame preparation reports Fatal after a presentation acquisition has been obtained
- **THEN** the product does not open a graphics list or start another fallback, releases CPU borrows and resolves the acquisition through legal shutdown ownership, and returns status 1 without waiting on a fabricated completion value
