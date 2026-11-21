# opengl-device-resources Spec Delta

## MODIFIED Requirements

### Requirement: Expose actual resource capabilities conservatively
Reported resource support SHALL be the intersection of the implemented resource profile and actual device, target, format and usage observations. Creation SHALL enforce the same limits, including supported sample counts rather than only a maximum. Metadata and allocation support SHALL NOT imply an implemented shader, binding or draw path. Unavailable feature queries SHALL preserve caller output.

Storage image 的 `ShaderUavLoad` 和 `ShaderUavStore` SHALL 按实际目标与格式区分，设备和图能力查询 SHALL 只对已交付且实测能力允许的组合返回支持。直接 Compute Dispatch 的可用性 SHALL 不改变 `ComputeQueue` 的不支持结果。

#### Scenario: Query mapped and unmapped formats
- **WHEN** a caller queries a mapped format, a known but excluded format or an unknown value
- **THEN** only supported mapped usages receive positive flags and excluded or unknown formats report no support

#### Scenario: Reject a sample count absent from the supported set
- **WHEN** a requested multisample count is absent from the actual format and target's permitted sample set
- **THEN** creation rejects it even if the count is below a reported global maximum

#### Scenario: Enforce actual dimensions and attachment limits
- **WHEN** a resource exceeds an applicable device dimension, layer, representable size or attachment limit
- **THEN** it fails before native allocation rather than truncating or clamping the request

#### Scenario: Query a feature without a delivered operation
- **WHEN** the underlying API has facilities for a feature whose RHI operation is not implemented in this profile
- **THEN** the device and its validation layer report no enabled support and leave optional output data unchanged

#### Scenario: Report storage image access independently of sampling

- **WHEN** 查询普通采样支持与已定义的 storage image 格式，并以目标、格式、读写方向核对实际能力
- **THEN** 仅被实现并得到设备支持的 storage 方向获得对应 flag。只有采样支持的格式仍拒绝 storage 用途

### Requirement: Create immutable buffer allocations

A valid supported ordinary buffer description SHALL produce one device-owned immutable allocation with the requested byte capacity and preserved descriptor meaning. The ordinary buffer profile SHALL include the `isDrawIndirectArgs` role for supported indexed indirect commands, subject to ordinary allocation and transfer constraints. A supported volatile constant-buffer description SHALL retain its public logical byte capacity and nonzero `maxVersions`, with storage for that many aligned versions under the actual device limits and a finite device budget. Size conversions, stride multiplication, applicable UBO and mapping alignments, and layout constraints SHALL be checked before allocation. CPU mapping, typed views and volatile uses outside the constant-buffer profile SHALL be rejected. Allocating storage SHALL NOT initialize its contents or represent a completed upload.

普通有结构化 stride 的 Buffer 在显式设置 `canHaveUAVs` 时 SHALL 支持已交付的 StructuredBuffer_UAV。符合各角色约束的 Buffer SHALL 可以同时用于结构化计算输出和已支持的 indexed indirect 参数消费。Buffer allocation 成功本身不赋予 typed/raw UAV、volatile 写入或其它未经交付的绑定用途。

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

#### Scenario: Allocate structured output for indexed arguments

- **WHEN** 请求带有效结构 stride、`canHaveUAVs` 和 `isDrawIndirectArgs` 的普通 Buffer，并在支持的范围内构建各自绑定
- **THEN** 返回描述与原生不可变容量匹配，同一存储接受经过检查的结构化写入及 indexed indirect 消费请求

### Requirement: Create texture storage with explicit shape and usage
Valid supported textures SHALL retain their dimensions, mip and array semantics, sample count, usage and color encoding. Storage and creation checks SHALL use the actual target and format. Unsupported usages or formats SHALL fail without conversion to another resource. Initial-state and clear-value metadata SHALL NOT be interpreted as initialized pixels or completed transitions.

装配 storage image 的 `isUAV` SHALL 只接受实现所列单样本、非压缩、同格式颜色 Texture2D 或 Texture2DArray。候选格式为 `R32_FLOAT`、`R32_UINT`、`RGBA8_UNORM` 与 `RGBA16_FLOAT`，每个目标仍须有对应设备的写能力。具体 image 读写方向 SHALL 在绑定消费时按其要求核对 load/store 能力。

#### Scenario: Allocate supported texture dimensions
- **WHEN** a caller creates valid one-dimensional, two-dimensional, three-dimensional, array, cube or permitted multisample storage
- **THEN** native storage queries match the effective mip dimensions, array or face count, sample count and sized format

#### Scenario: Reject an incompatible shape
- **WHEN** a request has an invalid cube face count, incompatible dimension fields, excessive mips or an invalid sample and mip combination
- **THEN** it is rejected before native storage creation

#### Scenario: Keep data and sRGB storage distinct
- **WHEN** otherwise comparable textures request supported linear data and sRGB formats
- **THEN** their native storage and reported format retain that distinction without modifying bytes or silently selecting another encoding

#### Scenario: Preserve uninitialized-content semantics
- **WHEN** storage is created with initial-state or clear-value metadata and no content operation has occurred
- **THEN** the resource is not reported as containing uploaded or cleared pixels and no state transition or GPU completion is invented

#### Scenario: Constrain storage texture creation to the delivered profile

- **WHEN** 请求一个已支持的线性颜色 storage 目标，同时用 sRGB、深度、多样本或缺少目标写能力的描述作对照
- **THEN** 合法描述保留准确 mip/layer、编码及 storage 用途，非法描述在分配前明确拒绝

### Requirement: Reject unavailable operations without fictitious GPU work

The delivered device SHALL preserve the complete public interface and distinguish documented resource/upload operations, baseline Shader, BindingLayout/BindingSet, InputLayout and GraphicsPipeline preparation, limited immediate graphics execution specified by opengl-immediate-draw, direct compute execution specified by opengl-immediate-compute, and Event/Timer queries specified by opengl-completion-queries from unavailable commands, readbacks and extensions. Preparation SHALL remain limited to opengl-shader-pipelines. Supported direct graphics, fixed-count indexed indirect, direct compute commands and queries SHALL require their documented profile, actual entry points, ownership and state. Successful resource allocation or native program link SHALL NOT establish execution support or full rendering acceptance.

Unsupported calls SHALL preserve output data where required and SHALL NOT create successful dummy objects, issue work or tickets, or wait for invented GPU work. Nonindexed indirect draws, indirect count-buffer commands, indirect dispatch, storage access outside the delivered compute profile, public staging/map or asynchronous readback, other query extensions, multisample rendering and functionality outside the documented profiles SHALL remain unavailable through this capability.

#### Scenario: Request a command list or submission
- **WHEN** a caller requests a list or submission mode outside the documented upload and immediate Graphics queue profiles
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

#### Scenario: Execute compute on the owning graphics recording

- **WHEN** 既有 Graphics 即时列表使用合法 Compute Pipeline 执行直接 Dispatch，并对照请求间接 Dispatch 或专用 Compute 队列
- **THEN** 直接 Dispatch 发出真实工作。间接 Dispatch 和专用队列按现有失败形式明确拒绝
