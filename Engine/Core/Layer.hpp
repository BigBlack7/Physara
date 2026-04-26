#pragma once

namespace Physara::Engine
{
    class Layer
    {
    public:
        virtual ~Layer() = default;

        // 出入栈行为
        virtual void OnAttach() {}
        virtual void OnDetach() {}

        // 每帧行为
        virtual void OnUpdate(float deltaTime) {}
        virtual void OnUIRender() {}
    };
}