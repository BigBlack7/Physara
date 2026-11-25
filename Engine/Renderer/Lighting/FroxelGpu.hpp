#pragma once

/// @file
/// @brief 将一份 View 的 Froxel 候选交给帧上传包, 由实际受光 Pass 读取图内写出的版本.

#include <array>
#include <map>
#include <memory>
#include <optional>

#include "../RenderGraph/GraphExecutor.hpp"

namespace Physara::Renderer
{
    struct PreparedView;

    namespace Detail
    {
        struct Allocation;
        struct ForwardDevice;
        struct UploadBatch;

        /// @brief 同一图中写出的 PerView b1 与 t0 至 t2. 调用方合入当前 View 的 b0 和其它受光输入.
        class FroxelGraphInputs final
        {
        public:
            /// @brief 声明对四个完整活动块的读依赖, 让图决定上传活性和实际转换.
            [[nodiscard]] bool DeclareReads(PassBuilder &builder) const;
            /// @brief 从同一 Pass 的读权限得到绑定条目. 失败时准备器保留本轮错误.
            [[nodiscard]] std::optional<std::array<RHI::BindingSetItem, 4>> Bind(PassPreparation &preparation) const;

        private:
            friend class FroxelUpload;
            std::array<BufferId, 4> m_Versions;
        };

        /// @brief 当前冻结 View 的一次 GPU 上传准备. 可消费数据只在原图声明写版本后获得.
        /// @note 批次持有源字节和目标 Buffer, 本对象持有同组存储的预算凭据.
        /// 调用方可在录制结束或放弃后释放载荷. 若命令已提交, 所属帧槽须保留预算凭据至实际完成.
        /// 正常提交和失败前缀的原生引用由 RHI 保活, 无录制的候选可立即释放.
        class FroxelUpload final
        {
        public:
            /// @brief 追加一组拥有型 Buffer 写入. 无可消费 CPU 网格时返回空, 完整灯表可独立使用.
            /// @pre device 已选中该 View 的准备预算, limits 来自同设备的图 profile. 批次尚未封存, CPU 候选仍在借用期内.
            /// @throws ForwardFailure 设备分配或预算拒绝. 失败时不向批次发布部分写入.
            /// @throws std::bad_alloc CPU 载荷或批次容器无法分配, 由帧准备入口处理.
            [[nodiscard]] static std::optional<FroxelUpload> Prepare(ForwardDevice &device, UploadBatch &batch,
                                                                     const PreparedView &view,
                                                                     const GraphBufferSupport &limits);

            /// @brief 从场景上传声明的结果取得本组四个写版本, 供同一图的受光 Pass 声明读依赖.
            /// @pre batch 是 Prepare 成功追加的已封存帧批次, versions 来自本图对该批次的上传声明.
            /// @return 缺少本组载荷或写版本时返回错误, 图声明保持不变.
            [[nodiscard]] GraphResult<FroxelGraphInputs> Resolve(const UploadBatch &batch,
                const std::map<RHI::Buffer *, BufferId> &versions) const noexcept;

        private:
            explicit FroxelUpload(std::shared_ptr<Allocation> allocation) noexcept;
            std::shared_ptr<Allocation> m_Allocation;
        };
    }
}
