/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#pragma once

/// @file
/// @brief 可组合 Validation 的公共工厂, 输入规则和包装状态封闭在私有实现中.

#include "RHI.hpp"

namespace Physara::RHI::Validation
{
    /// @brief 借用底层 Device 并返回强持有它的包装, null Device/callback 或无效 API 返回空.
    /// @note callback 覆盖全部包装对象, 不抛异常, 不重入; 设备沿用底层线程合同, 列表为单写者.
    /// 工厂 bad_alloc 转为空, 普通 CPU 容器异常可传播, 包装引用不表示 GPU 完成.
    PHYSARA_RHI_API DeviceHandle CreateValidationLayer(Device *underlyingDevice);
}
