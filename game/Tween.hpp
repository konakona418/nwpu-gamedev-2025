#pragma once

#include "Core/Common.hpp"
#include "Math/Util.hpp"

namespace game {
    struct LinearProvider {
        static float value(float factor) { return factor; }
    };

    struct QuadraticProvider {
        static float value(float factor) { return factor * factor; }
    };

    struct CubicProvider {
        static float value(float factor) { return factor * factor * factor; }
    };

    template<typename EaseInProvider, typename EaseOutProvider>
    struct Tween {
    private:
        float m_inPoint;
        float m_outPoint;

    public:
        Tween(float inDuration, float outDuration) {
            m_inPoint = moe::Math::clamp(inDuration, 0.0f, 1.0f);
            m_outPoint = moe::Math::clamp(1.0f - outDuration, m_inPoint, 1.0f);
        }

        float eval(float t) {
            t = moe::Math::clamp(t, 0.0f, 1.0f);

            if (t < m_inPoint && m_inPoint > 0.0f) {
                float localFactor = t / m_inPoint;
                return EaseInProvider::value(localFactor);
            }

            if (t <= m_outPoint) {
                return 1.0f;
            }

            if (m_outPoint < 1.0f) {
                float localFactor = (t - m_outPoint) / (1.0f - m_outPoint);
                return EaseOutProvider::value(1.0f - localFactor);
            }

            return 0.0f;
        }
    };
}// namespace game