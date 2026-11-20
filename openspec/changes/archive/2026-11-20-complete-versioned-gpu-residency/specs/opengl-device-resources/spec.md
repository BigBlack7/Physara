# Spec Delta

## MODIFIED Requirements

### Requirement: Create immutable buffer allocations

A valid supported ordinary buffer description SHALL produce one device-owned immutable allocation with the requested byte capacity and preserved descriptor meaning. A supported volatile constant-buffer description SHALL retain its public logical byte capacity and nonzero `maxVersions`, with storage for that many aligned versions under the actual device limits and a finite device budget. Size conversions, stride multiplication, applicable UBO and mapping alignments, and layout constraints SHALL be checked before allocation. CPU mapping, typed views and volatile uses outside the constant-buffer profile SHALL be rejected. Allocating storage SHALL NOT initialize its contents or represent a completed upload.

#### Scenario: Create and inspect an ordinary buffer
- **WHEN** a caller requests a supported nonzero buffer allocation
- **THEN** the returned descriptor and native allocation query agree on the requested byte capacity and immutable storage

#### Scenario: Reject an unrepresentable buffer allocation
- **WHEN** the requested capacity is zero, cannot be represented by native allocation parameters or violates a declared structured or raw layout
- **THEN** creation reports the invalid request before allocating a native object

#### Scenario: Request unavailable buffer access
- **WHEN** a caller requests CPU mapping, a typed buffer view or a volatile use outside the supported constant-buffer profile
- **THEN** the request fails explicitly without substituting ordinary storage or reporting a successful upload

#### Scenario: Allocate bounded volatile constant storage
- **WHEN** a caller requests a supported volatile constant buffer with a small nonzero `maxVersions`, then requests one whose aligned backing exceeds the allowed capacity
- **THEN** the accepted descriptor reports its logical size and version count and starts without a usable written version; the excessive request fails before native allocation with a specific limit diagnostic
