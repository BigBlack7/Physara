# Spec Delta

## MODIFIED Requirements

### Requirement: Release native ownership completely and repeatably
On the owner thread, shutdown and rollback SHALL release acquired state in reverse dependency order, disconnect borrowed callbacks before storage dies and restore the prior process error callback. Successful shutdown SHALL be idempotent and allow a new host; a stopped owner SHALL NOT release a newer host. Genuine external RHI owners, including caller-owned lists and resources, SHALL cause ResourceInUse before teardown and permit retry after release. Internal upload retention alone SHALL instead be gathered under the owning context before release. An incomplete drain SHALL report a distinct retryable result and retain the native environment while new work remains disabled.

#### Scenario: Repeat successful create and shutdown
- **WHEN** a caller repeatedly creates a real host, explicitly shuts it down and destroys the stopped owner
- **THEN** each iteration releases its window, context and process ownership and the next iteration can initialize successfully

#### Scenario: Retry after failed creation
- **WHEN** a controlled creation failure has been rolled back successfully and the caller then makes a valid request
- **THEN** the valid request can create a real host without stale initialization or callback state

#### Scenario: Restore callback ownership
- **WHEN** a callback existed before the host acquired native ownership and the host subsequently shuts down or rolls back
- **THEN** that callback is restored and later native errors cannot access the destroyed host's error storage

#### Scenario: Shut down a stopped owner after replacement
- **WHEN** a stopped owner remains alive, a new host is created, and shutdown is called again on the old owner
- **THEN** the operation is an idempotent success and leaves the new host intact

#### Scenario: Reject shutdown while a resource is live
- **WHEN** explicit shutdown is requested while an externally owned buffer, texture, sampler or framebuffer remains alive
- **THEN** it reports ResourceInUse before destroying the device, callbacks, input or native environment and a later retry can succeed after those resources are released

#### Scenario: Reject shutdown while the device is externally retained
- **WHEN** explicit shutdown is requested while a caller owns an extra device or validation-layer reference
- **THEN** it reports ResourceInUse without invalidating that reference or its callback target and permits retry after the reference is released

#### Scenario: Drain final releases before closing the context
- **WHEN** resource users have stopped, final releases have been queued and no external RHI owner remains
- **THEN** shutdown drains native deletions with the owning context current, releases validation before its raw device and releases the callback target after device destruction

#### Scenario: Close with only internal upload references
- **WHEN** callers have released all resources, lists and extra devices while submitted or abandoned uploads retain internal references
- **THEN** shutdown gathers the real issued work without misclassifying internal holds as an external ResourceInUse failure

#### Scenario: Keep the native owner after a drain timeout
- **WHEN** a shutdown drain deadline expires while a valid fence is still pending
- **THEN** the host reports the distinct incomplete-drain reason, stops offering new device/input work, retains the device, callback, context, window and process claim, and permits a later shutdown retry

#### Scenario: Gather an unfenced prefix during final exit
- **WHEN** issued uploads have no reliable fence or completion query and the host must close
- **THEN** it uses the documented usable-context exit gathering or unavailable-context destruction path, preserves the original failure and never publishes a false completion
