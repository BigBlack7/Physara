#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace Physara::Engine
{
    struct TagComponent
    {
        std::string name{"Entity"};

        TagComponent() = default;
        explicit TagComponent(std::string_view entityName) : name(entityName) {}
        explicit TagComponent(std::string entityName) : name(std::move(entityName)) {}
    };
}