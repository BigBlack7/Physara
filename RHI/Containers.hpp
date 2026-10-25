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

// 来源与受控差异见 RHI/README.md; 此头不代表后端能力已实现.

#include <array>
#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace Physara::RHI
{

    // 容量/索引和元素操作须满足前置条件; pop 不销毁保留槽, resize 按源合同重置槽.
    // a static vector is a vector with a capacity defined at compile-time
    template <typename T, uint32_t Capacity> struct StaticVector : private std::array<T, Capacity>
    {
        typedef std::array<T, Capacity> Base;
        enum
        {
            MaxElements = Capacity
        };

        using typename Base::value_type;
        using typename Base::size_type;
        using typename Base::difference_type;
        using typename Base::reference;
        using typename Base::const_reference;
        using typename Base::pointer;
        using typename Base::const_pointer;
        using typename Base::iterator;
        using typename Base::const_iterator;
        // xxxnsubtil: reverse iterators not implemented

        StaticVector() : Base(), m_CurrentSize(0)
        {
        }

        StaticVector(size_t size) : Base(), m_CurrentSize(size)
        {
            assert(size <= MaxElements);
        }

        StaticVector(std::initializer_list<T> il) : Base(), m_CurrentSize(0)
        {
            for (auto i : il)
            {
                push_back(i);
            }
        }

        using Base::at;

        reference operator[](size_type pos)
        {
            assert(pos < m_CurrentSize);
            return Base::operator[](pos);
        }

        const_reference operator[](size_type pos) const
        {
            assert(pos < m_CurrentSize);
            return Base::operator[](pos);
        }

        using Base::front;

        reference back() noexcept
        {
            auto tmp = end();
            --tmp;
            return *tmp;
        }
        const_reference back() const noexcept
        {
            auto tmp = cend();
            --tmp;
            return *tmp;
        }

        using Base::data;
        using Base::begin;
        using Base::cbegin;

        iterator end() noexcept
        {
            return iterator(begin()) + m_CurrentSize;
        }
        const_iterator end() const noexcept
        {
            return cend();
        }
        const_iterator cend() const noexcept
        {
            return const_iterator(cbegin()) + m_CurrentSize;
        }

        bool empty() const noexcept
        {
            return m_CurrentSize == 0;
        }
        size_t size() const noexcept
        {
            return m_CurrentSize;
        }
        constexpr size_t max_size() const noexcept
        {
            return MaxElements;
        }

        void fill(const T &value) noexcept
        {
            Base::fill(value);
            m_CurrentSize = MaxElements;
        }

        void swap(StaticVector &other) noexcept
        {
            Base::swap(other);
            std::swap(m_CurrentSize, other.m_CurrentSize);
        }

        void push_back(const T &value) noexcept
        {
            assert(m_CurrentSize < MaxElements);
            *(data() + m_CurrentSize) = value;
            m_CurrentSize++;
        }

        void push_back(T &&value) noexcept
        {
            assert(m_CurrentSize < MaxElements);
            *(data() + m_CurrentSize) = std::move(value);
            m_CurrentSize++;
        }

        void pop_back() noexcept
        {
            assert(m_CurrentSize > 0);
            m_CurrentSize--;
        }

        void resize(size_type new_size) noexcept
        {
            assert(new_size <= MaxElements);

            if (m_CurrentSize > new_size)
            {
                for (size_type i = new_size; i < m_CurrentSize; i++)
                {
                    *(data() + i) = T{};
                }
            }
            else
            {
                for (size_type i = m_CurrentSize; i < new_size; i++)
                {
                    *(data() + i) = T{};
                }
            }

            m_CurrentSize = new_size;
        }

        reference emplace_back() noexcept
        {
            assert(m_CurrentSize < MaxElements);
            ++m_CurrentSize;
            back() = T{};
            return back();
        }

      private:
        size_type m_CurrentSize = 0;
    };

} // namespace nvrhi
