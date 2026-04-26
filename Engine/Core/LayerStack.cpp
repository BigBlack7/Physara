#include "LayerStack.hpp"

#include <algorithm>

#include <Engine/Core/Layer.hpp>

namespace Physara::Engine
{
    void LayerStack::PushLayer(Layer *layer)
    {
        if (layer == nullptr)
        {
            return;
        }

        const auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
        if (it != m_Layers.end())
        {
            return;
        }

        m_Layers.push_back(layer);
        layer->OnAttach();
    }

    void LayerStack::PopLayer(Layer *layer)
    {
        if (layer == nullptr)
        {
            return;
        }

        const auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
        if (it == m_Layers.end())
        {
            return;
        }

        (*it)->OnDetach();
        m_Layers.erase(it);
    }
}