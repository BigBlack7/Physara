# Spec Delta

## MODIFIED Requirements

### Requirement: Expose actual resource capabilities conservatively
Reported resource support SHALL be the intersection of the implemented resource profile and actual device, target, format and usage observations. Creation SHALL enforce the same limits, including supported sample counts rather than only a maximum. Metadata and allocation support SHALL NOT imply an implemented shader, binding or draw path. Unavailable feature queries SHALL preserve caller output.

Storage image 的 `ShaderUavLoad` 和 `ShaderUavStore` SHALL 按实际目标与格式区分，设备和图能力查询 SHALL 只对已交付且实测能力允许的组合返回支持。直接 Compute Dispatch 的可用性 SHALL 不改变 `ComputeQueue` 的不支持结果。

实际传输支持 SHALL 分别约束 clear、普通 copy、staging 读写、mip 及 resolve 的格式、目标和样本。Resolve 支持 SHALL 以源和目标的实测能力、首批格式 profile 及对应实际源样本集合为前提。设备与直接消费者 SHALL 使用一致的能力依据，分配、采样或单一样本最大值均不足以证明其他用途。

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

#### Scenario: Distinguish staged or resolved access from allocation
- **WHEN** 比较普通分配、带公开 staging 支持的格式，以及可分配但缺少对应 resolve 组合能力的多样本目标
- **THEN** 各操作只对其实际交付的格式、目标和样本组合报告支持，合法能力用于各自调用，只有分配支持的对照仍有明确传输拒绝

### Requirement: Create immutable buffer allocations

A valid supported ordinary buffer description SHALL produce one device-owned immutable allocation with the requested byte capacity and preserved descriptor meaning. The ordinary buffer profile SHALL include the `isDrawIndirectArgs` role for supported indexed indirect commands, subject to ordinary allocation and transfer constraints. A supported volatile constant-buffer description SHALL retain its public logical byte capacity and nonzero `maxVersions`, with storage for that many aligned versions under the actual device limits and a finite device budget. Size conversions, stride multiplication, applicable UBO and mapping alignments, and layout constraints SHALL be checked before allocation. Typed views and volatile uses outside the constant-buffer profile SHALL be rejected. Allocating storage SHALL NOT initialize its contents or represent a completed upload.

普通有结构化 stride 的 Buffer 在显式设置 `canHaveUAVs` 时 SHALL 支持已交付的 StructuredBuffer_UAV。符合各角色约束的 Buffer SHALL 可以同时用于结构化计算输出和已支持的 indexed indirect 参数消费。Buffer allocation 成功本身不赋予 typed/raw UAV、volatile 写入或其它未经交付的绑定用途。

专用 staging Buffer SHALL 接受 `CpuAccessMode::Read` 或 `Write`，保留请求的方向和逻辑字节容量，遵循 `opengl-immediate-transfer` 的映射、复制及预算合同。普通或 volatile Buffer 以及混入 GPU 绑定用途的描述 SHALL 不取得此 CPU 映射支持。

#### Scenario: Create and inspect an ordinary buffer
- **WHEN** a caller requests a supported nonzero buffer allocation
- **THEN** the returned descriptor and native allocation query agree on the requested byte capacity and immutable storage

#### Scenario: Reject an unrepresentable buffer allocation
- **WHEN** the requested capacity is zero, cannot be represented by native allocation parameters or violates a declared structured or raw layout
- **THEN** creation reports the invalid request before allocating a native object

#### Scenario: Request unavailable buffer access
- **WHEN** a caller requests CPU mapping of ordinary or volatile storage, mixes staging with a GPU binding role, requests a typed buffer view or selects a volatile use outside the supported constant-buffer profile
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

#### Scenario: Create a dedicated buffer with a fixed CPU direction
- **WHEN** 合法描述请求单独的 Read 或 Write staging 容量
- **THEN** 返回描述保留方向和 byteSize，初次分配未发表 Read 数据，复制与映射仍各自按方向和完成条件准入

### Requirement: Reject unavailable operations without fictitious GPU work

The delivered device SHALL preserve the complete public interface and distinguish documented resource/upload operations, baseline Shader, BindingLayout/BindingSet, InputLayout and GraphicsPipeline preparation, limited immediate graphics execution specified by opengl-immediate-draw, direct compute execution specified by opengl-immediate-compute, the transfer and staging operations specified by opengl-immediate-transfer, and Event/Timer queries specified by opengl-completion-queries from unavailable commands and extensions. Preparation SHALL remain limited to opengl-shader-pipelines. Supported direct graphics, fixed-count indexed indirect, direct compute commands, transfers, maps and queries SHALL require their documented profile, actual entry points, ownership and state. Successful resource allocation or native program link SHALL NOT establish execution support or full rendering acceptance.

Unsupported calls SHALL preserve output data where required and SHALL NOT create successful dummy objects, issue work or tickets, or wait for invented GPU work. Nonindexed indirect draws, indirect count-buffer commands, indirect dispatch, storage access outside the delivered compute profile, staging modes outside the delivered profile, query extensions, multisample Draw and functionality outside the documented profiles SHALL remain unavailable through this capability.

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
- **WHEN** a caller uses a supported Event/Timer query or staging transfer and also requests an excluded query or readback mode
- **THEN** supported calls follow their own ownership and readiness contract, and the excluded operation fails without fabricated pixels, row pitch or completion

#### Scenario: Execute compute on the owning graphics recording

- **WHEN** 既有 Graphics 即时列表使用合法 Compute Pipeline 执行直接 Dispatch，并对照请求间接 Dispatch 或专用 Compute 队列
- **THEN** 直接 Dispatch 发出真实工作。间接 Dispatch 和专用队列按现有失败形式明确拒绝
