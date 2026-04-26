#pragma once

#include <vector>

namespace Physara::Engine
{
    class Layer;

    class LayerStack
    {
    public:
        LayerStack() = default;
        ~LayerStack() = default;

        void PushLayer(Layer *layer);
        void PopLayer(Layer *layer);

        std::vector<Layer *>::iterator Begin() { return m_Layers.begin(); }
        std::vector<Layer *>::iterator End() { return m_Layers.end(); }

        std::vector<Layer *>::const_iterator Begin() const { return m_Layers.begin(); }
        std::vector<Layer *>::const_iterator End() const { return m_Layers.end(); }

    private:
        std::vector<Layer *> m_Layers{};
    };
}