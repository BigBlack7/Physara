# Spec Delta

## MODIFIED Requirements

### Requirement: Create immutable buffer allocations

A valid supported ordinary buffer description SHALL produce one device-owned immutable allocation with the requested byte capacity and preserved descriptor meaning. The ordinary buffer profile SHALL include the `isDrawIndirectArgs` role for supported indexed indirect commands, subject to ordinary allocation and transfer constraints. A supported volatile constant-buffer description SHALL retain its public logical byte capacity and nonzero `maxVersions`, with storage for that many aligned versions under the actual device limits and a finite device budget. Size conversions, stride multiplication, applicable UBO and mapping alignments, and layout constraints SHALL be checked before allocation. CPU mapping, typed views and volatile uses outside the constant-buffer profile SHALL be rejected. Allocating storage SHALL NOT initialize its contents or represent a completed upload.

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
- **THEN** the accepted descriptor reports its logical size and version count and starts without a usable written version, and the excessive request fails before native allocation with a specific limit diagnostic

#### Scenario: Allocate storage for explicit indexed argument uploads
- **WHEN** a caller creates a supported ordinary Buffer with `isDrawIndirectArgs` and uses the existing transfer operations to upload indexed argument records to a valid range
- **THEN** both creation and transfer preserve the requested capacity, argument flag and byte range, and availability of the records follows the actual write and its access dependency

### Requirement: Reject unavailable operations without fictitious GPU work

The delivered device SHALL preserve the complete public interface and distinguish documented resource/upload operations, baseline Shader, BindingLayout/BindingSet, InputLayout and GraphicsPipeline preparation, limited immediate graphics execution specified by opengl-immediate-draw, and Event/Timer queries specified by opengl-completion-queries from unavailable commands, readbacks and extensions. Preparation SHALL remain limited to opengl-shader-pipelines. Supported direct and fixed-count indexed indirect graphics commands and queries SHALL require their documented profile, actual entry points, ownership and state. Successful resource allocation or native program link SHALL NOT establish execution support or full rendering acceptance.

Unsupported calls SHALL preserve output data where required and SHALL NOT create successful dummy objects, issue work or tickets, or wait for invented GPU work. Nonindexed indirect draws, indirect count-buffer commands, Compute/UAV, public staging/map or asynchronous readback, other query extensions, multisample rendering and functionality outside the documented profiles SHALL remain unavailable through this capability.

#### Scenario: Request a command list or submission
- **WHEN** a caller requests a list or submission mode outside the documented upload and immediate graphics profiles
- **THEN** the operation fails with no list, replay, issued work or positive submission result

#### Scenario: Request an excluded family with unused payload
- **WHEN** an unsupported extension operation is called with payload it does not need to inspect
- **THEN** it reports unsupported behavior before dereferencing the payload or forwarding native work

#### Scenario: Request an unavailable reference-only helper
- **WHEN** a caller requests the unsupported crash helper
- **THEN** the reason is reported and the documented logic-error failure occurs without returning a fake reference

#### Scenario: Separate supported preparation from unavailable drawing
- **WHEN** a baseline Shader, binding or graphics Pipeline is successfully prepared and the caller attempts `DrawIndirect` or `DrawIndexedIndirectCount`
- **THEN** the prepared object is real while the command fails visibly without a Draw, replay or fictitious submission ticket

#### Scenario: Execute only the delivered direct graphics profile
- **WHEN** a caller uses the documented single-sample direct graphics operations with valid prepared objects and an owning open immediate list
- **THEN** the operations perform real native work under opengl-immediate-draw while command families outside the delivered profiles remain explicitly unavailable

#### Scenario: Use queries without inferring a readback implementation
- **WHEN** supported Event/Timer queries are used and a caller separately requests public staging/map or an excluded readback/query extension
- **THEN** the queries follow opengl-completion-queries while the excluded operation remains explicitly unsupported, with no fabricated pixels, row pitch or completed request
