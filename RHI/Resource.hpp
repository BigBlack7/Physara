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
/// @brief 固定来源的原生互操作对象与 intrusive CPU 句柄, retain/copy 增加拥有权, adoption/move 转移拥有权.

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace Physara::RHI
{
    typedef uint32_t ObjectType;

    // ObjectTypes namespace contains identifiers for various object types.
    // All constants have to be distinct. Implementations of NVRHI may extend the list.
    //
    // The encoding is chosen to minimize potential conflicts between implementations.
    // 0x00aabbcc, where:
    //   aa is GAPI, 1 for D3D11, 2 for D3D12, 3 for VK
    //   bb is layer, 0 for native GAPI objects, 1 for reference NVRHI backend, 2 for user-defined backends
    //   cc is a sequential number

    namespace ObjectTypes
    {
        constexpr ObjectType SharedHandle = 0x00000001;

        constexpr ObjectType D3D11_Device = 0x00010001;
        constexpr ObjectType D3D11_DeviceContext = 0x00010002;
        constexpr ObjectType D3D11_Resource = 0x00010003;
        constexpr ObjectType D3D11_Buffer = 0x00010004;
        constexpr ObjectType D3D11_RenderTargetView = 0x00010005;
        constexpr ObjectType D3D11_DepthStencilView = 0x00010006;
        constexpr ObjectType D3D11_ShaderResourceView = 0x00010007;
        constexpr ObjectType D3D11_UnorderedAccessView = 0x00010008;

        constexpr ObjectType D3D12_Device = 0x00020001;
        constexpr ObjectType D3D12_CommandQueue = 0x00020002;
        constexpr ObjectType D3D12_GraphicsCommandList = 0x00020003;
        constexpr ObjectType D3D12_Resource = 0x00020004;
        constexpr ObjectType D3D12_RenderTargetViewDescriptor = 0x00020005;
        constexpr ObjectType D3D12_DepthStencilViewDescriptor = 0x00020006;
        constexpr ObjectType D3D12_ShaderResourceViewGpuDescriptor = 0x00020007;
        constexpr ObjectType D3D12_UnorderedAccessViewGpuDescriptor = 0x00020008;
        constexpr ObjectType D3D12_RootSignature = 0x00020009;
        constexpr ObjectType D3D12_PipelineState = 0x0002000a;
        constexpr ObjectType D3D12_CommandAllocator = 0x0002000b;

        constexpr ObjectType VK_Device = 0x00030001;
        constexpr ObjectType VK_PhysicalDevice = 0x00030002;
        constexpr ObjectType VK_Instance = 0x00030003;
        constexpr ObjectType VK_Queue = 0x00030004;
        constexpr ObjectType VK_CommandBuffer = 0x00030005;
        constexpr ObjectType VK_DeviceMemory = 0x00030006;
        constexpr ObjectType VK_Buffer = 0x00030007;
        constexpr ObjectType VK_Image = 0x00030008;
        constexpr ObjectType VK_ImageView = 0x00030009;
        constexpr ObjectType VK_AccelerationStructureKHR = 0x0003000a;
        constexpr ObjectType VK_Sampler = 0x0003000b;
        constexpr ObjectType VK_ShaderModule = 0x0003000c;
        [[deprecated]]
        constexpr ObjectType VK_RenderPass = 0x0003000d;
        [[deprecated]]
        constexpr ObjectType VK_Framebuffer = 0x0003000e;
        constexpr ObjectType VK_DescriptorPool = 0x0003000f;
        constexpr ObjectType VK_DescriptorSetLayout = 0x00030010;
        constexpr ObjectType VK_DescriptorSet = 0x00030011;
        constexpr ObjectType VK_PipelineLayout = 0x00030012;
        constexpr ObjectType VK_Pipeline = 0x00030013;
        constexpr ObjectType VK_Micromap = 0x00030014;
        constexpr ObjectType VK_ImageCreateInfo = 0x00030015;
    };

    /// @brief 原生整数或指针的非拥有互操作值, 活动表示由 ObjectType 决定, 不增加原生引用.
    struct Object
    {
        union
        {
            uint64_t integer;
            void *pointer;
        };

        Object(uint64_t i) : integer(i)
        {
        } // NOLINT(cppcoreguidelines-pro-type-member-init)
        Object(void *p) : pointer(p)
        {
        } // NOLINT(cppcoreguidelines-pro-type-member-init)

        template <typename T> operator T *() const
        {
            return static_cast<T *>(pointer);
        }
    };

    struct MemoryRequirements;

    /// @brief CPU 引用计数对象的基础协议, 活跃内容与原生销毁仍遵守所属设备和线程合同.
    class Resource
    {
      protected:
        Resource() = default;
        virtual ~Resource() = default;

      public:
        virtual unsigned long AddRef() = 0;
        virtual unsigned long Release() = 0;
        virtual unsigned long GetRefCount() = 0;

        // Returns a native object or interface, for example ID3D11Device*, or nullptr if the requested interface is unavailable.
        // Does *not* AddRef the returned interface.
        virtual Object GetNativeObject(ObjectType objectType)
        {
            (void)objectType;
            return nullptr;
        }

        // Optional backing-memory query. Returns false without changing output when unavailable.
        // See doc/memory-queries.md for supported resources and accounting limitations.
        virtual bool QueryMemoryRequirements(MemoryRequirements &outRequirements)
        {
            (void)outRequirements;
            return false;
        }

        // Non-copyable and non-movable
        Resource(const Resource &) = delete;
        Resource(const Resource &&) = delete;
        Resource &operator=(const Resource &) = delete;
        Resource &operator=(const Resource &&) = delete;
    };

    //////////////////////////////////////////////////////////////////////////
    // RefCountPtr
    // Mostly a copy of Microsoft::WRL::ComPtr<T>
    //////////////////////////////////////////////////////////////////////////

    /// @brief intrusive owning handle, 指针构造/复制 AddRef, 移动转交引用, Get/箭头仅借用.
    /// @note Attach/Create 接管已有引用, Detach 转交引用, CPU 保活不证明 GPU 已完成.
    template <typename T> class RefCountPtr
    {
      public:
        typedef T InterfaceType;

        template <bool b, typename U = void> struct EnableIf
        {
        };

        template <typename U> struct EnableIf<true, U>
        {
            typedef U type;
        };

      protected:
        InterfaceType *m_Pointer;
        template <class U> friend class RefCountPtr;

        void InternalAddRef() const noexcept
        {
            if (m_Pointer != nullptr)
            {
                m_Pointer->AddRef();
            }
        }

        unsigned long InternalRelease() noexcept
        {
            unsigned long ref = 0;
            T *temp = m_Pointer;

            if (temp != nullptr)
            {
                m_Pointer = nullptr;
                ref = temp->Release();
            }

            return ref;
        }

      public:
        RefCountPtr() noexcept : m_Pointer(nullptr)
        {
        }

        RefCountPtr(std::nullptr_t) noexcept : m_Pointer(nullptr)
        {
        }

        template <class U> RefCountPtr(U *other) noexcept : m_Pointer(other)
        {
            InternalAddRef();
        }

        RefCountPtr(const RefCountPtr &other) noexcept : m_Pointer(other.m_Pointer)
        {
            InternalAddRef();
        }

        // copy ctor that allows to instanatiate class when U* is convertible to T*
        template <class U>
        RefCountPtr(const RefCountPtr<U> &other,
                    typename std::enable_if<std::is_convertible<U *, T *>::value, void *>::type * = nullptr) noexcept
            : m_Pointer(other.m_Pointer)

        {
            InternalAddRef();
        }

        RefCountPtr(RefCountPtr &&other) noexcept : m_Pointer(nullptr)
        {
            if (this != reinterpret_cast<RefCountPtr *>(&reinterpret_cast<unsigned char &>(other)))
            {
                Swap(other);
            }
        }

        // Move ctor that allows instantiation of a class when U* is convertible to T*
        template <class U>
        RefCountPtr(RefCountPtr<U> &&other,
                    typename std::enable_if<std::is_convertible<U *, T *>::value, void *>::type * = nullptr) noexcept
            : m_Pointer(other.m_Pointer)
        {
            other.m_Pointer = nullptr;
        }

        ~RefCountPtr() noexcept
        {
            InternalRelease();
        }

        RefCountPtr &operator=(std::nullptr_t) noexcept
        {
            InternalRelease();
            return *this;
        }

        RefCountPtr &operator=(T *other) noexcept
        {
            if (m_Pointer != other)
            {
                RefCountPtr(other).Swap(*this);
            }
            return *this;
        }

        template <typename U> RefCountPtr &operator=(U *other) noexcept
        {
            RefCountPtr(other).Swap(*this);
            return *this;
        }

        RefCountPtr &operator=(const RefCountPtr &other) noexcept // NOLINT(bugprone-unhandled-self-assignment)
        {
            if (m_Pointer != other.m_Pointer)
            {
                RefCountPtr(other).Swap(*this);
            }
            return *this;
        }

        template <class U> RefCountPtr &operator=(const RefCountPtr<U> &other) noexcept
        {
            RefCountPtr(other).Swap(*this);
            return *this;
        }

        RefCountPtr &operator=(RefCountPtr &&other) noexcept
        {
            RefCountPtr(static_cast<RefCountPtr &&>(other)).Swap(*this);
            return *this;
        }

        template <class U> RefCountPtr &operator=(RefCountPtr<U> &&other) noexcept
        {
            RefCountPtr(static_cast<RefCountPtr<U> &&>(other)).Swap(*this);
            return *this;
        }

        void Swap(RefCountPtr &&r) noexcept
        {
            T *tmp = m_Pointer;
            m_Pointer = r.m_Pointer;
            r.m_Pointer = tmp;
        }

        void Swap(RefCountPtr &r) noexcept
        {
            T *tmp = m_Pointer;
            m_Pointer = r.m_Pointer;
            r.m_Pointer = tmp;
        }

        [[nodiscard]] T *Get() const noexcept
        {
            return m_Pointer;
        }

        operator T *() const
        {
            return m_Pointer;
        }

        InterfaceType *operator->() const noexcept
        {
            return m_Pointer;
        }

        T **operator&() // NOLINT(google-runtime-operator)
        {
            return ReleaseAndGetAddressOf();
        }

        [[nodiscard]] T *const *GetAddressOf() const noexcept
        {
            return &m_Pointer;
        }

        [[nodiscard]] T **GetAddressOf() noexcept
        {
            return &m_Pointer;
        }

        /// @brief 先释放旧引用再借出内部指针槽供写入 owning 结果, operator& 使用同一语义.
        [[nodiscard]] T **ReleaseAndGetAddressOf() noexcept
        {
            InternalRelease();
            return &m_Pointer;
        }

        /// @brief 转交已有引用并将本 handle 置空, 不调用 Release, 接收方负责归还引用.
        T *Detach() noexcept
        {
            T *ptr = m_Pointer;
            m_Pointer = nullptr;
            return ptr;
        }

        // Set the pointer while keeping the object's reference count unchanged
        void Attach(InterfaceType *other)
        {
            if (m_Pointer != nullptr)
            {
                auto ref = m_Pointer->Release();
                (void)ref;

                // Attaching to the same object only works if duplicate references are being coalesced. Otherwise
                // re-attaching will cause the pointer to be released and may cause a crash on a subsequent dereference.
                assert(ref != 0 || m_Pointer != other);
            }

            m_Pointer = other;
        }

        // Create a wrapper around a raw object while keeping the object's reference count unchanged
        static RefCountPtr<T> Create(T *other)
        {
            RefCountPtr<T> ptr;
            ptr.Attach(other);
            return ptr;
        }

        unsigned long Reset()
        {
            return InternalRelease();
        }
    }; // RefCountPtr

    typedef RefCountPtr<Resource> ResourceHandle;

    //////////////////////////////////////////////////////////////////////////
    // RefCounter<T>
    // A class that implements reference counting in a way compatible with RefCountPtr.
    // Intended usage is to use it as a base class for interface implementations, like so:
    // class Texture : public RefCounter<ITexture> { ... }
    //////////////////////////////////////////////////////////////////////////

    /// @brief 初始计数为一的 CPU 引用实现, 原子计数只保护引用数, 不使对象的可变内容自动线程安全.
    template <class T> class RefCounter : public T
    {
      private:
        std::atomic<unsigned long> m_RefCount = 1;

      public:
        virtual unsigned long AddRef() override
        {
            return ++m_RefCount;
        }

        virtual unsigned long Release() override
        {
            unsigned long result = --m_RefCount;
            if (result == 0)
            {
                delete this;
            }
            return result;
        }

        virtual unsigned long GetRefCount() override
        {
            return m_RefCount.load();
        }
    };

} // namespace Physara::RHI
