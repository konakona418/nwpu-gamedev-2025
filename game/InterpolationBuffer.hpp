#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/TypeTraits.hpp"
#include "Math/Util.hpp"
#include "Physics/PhysicsEngine.hpp"

#include "RingBuffer.hpp"

namespace game {
    namespace Detail {
        template<typename T>
        struct InterpolatableValue {
        private:
            template<typename U>
            static auto test(int)
                    -> decltype(U::interpolate(
                                        std::declval<const U&>(),
                                        std::declval<const U&>(),
                                        0.5f),
                                std::true_type{});

            template<typename U>
            static auto test(...) -> std::false_type;

        public:
            static constexpr bool value = decltype(test<T>(0))::value;
        };

        template<typename T>
        constexpr bool IsInterpolatableV = InterpolatableValue<T>::value;
    };// namespace Detail

    template<
            typename T, size_t BufferSize = 64,
            typename = moe::Meta::EnableIfT<Detail::IsInterpolatableV<T>>>
    struct InterpolationBuffer {
    public:
        static constexpr size_t bufferSize = BufferSize;

        struct BufferItem {
            uint64_t physicsTick{0};
            T value;
        };

        explicit InterpolationBuffer(float interpolationDelayTicks = 2.0f)
            : m_interpolationDelayTicks(interpolationDelayTicks) {}

        void pushBack(const BufferItem& item) {
            m_buffer.pushBack(item);
        }

        void pushBack(T value, uint64_t physicsTick) {
            pushBack(BufferItem{
                    .physicsTick = physicsTick,
                    .value = value,
            });
        }

        T interpolate(float deltaTimeSecs) {
            if (m_buffer.empty()) {
                return T{};
            }

            if (m_buffer.size() == 1) {
                return m_buffer.front().value;
            }

            float targetTick = m_buffer.back().physicsTick - m_interpolationDelayTicks;
            if (m_renderTick < 0.01f || std::abs(m_renderTick - targetTick) > 10.f) {
                // not initialized or too far away, snap to target
                m_renderTick = targetTick;
            }

            float physicsTimestep = moe::PhysicsEngine::PHYSICS_TIMESTEP.count();
            m_renderTick += deltaTimeSecs * (1.0f / physicsTimestep);

            // clamp render tick
            if (m_renderTick < m_buffer.front().physicsTick) {
                m_renderTick = (float) m_buffer.front().physicsTick;
            }

            if (m_renderTick > targetTick) {
                m_renderTick = targetTick;
            }

            return _interpolate();
        }

    private:
        game::RingBuffer<BufferItem, bufferSize> m_buffer;
        float m_interpolationDelayTicks{2.0f};

        float m_renderTick{0.0f};

        T _interpolate() {
            // handled before, but just in case
            if (m_buffer.size() < 2) {
                return m_buffer.empty() ? T{} : m_buffer.front().value;
            }

            // find two buffer items to interpolate between
            BufferItem *itemA = nullptr, *itemB = nullptr;
            for (size_t i = 0; i < m_buffer.size() - 1; ++i) {
                if (m_renderTick >= (float) m_buffer[i + 1].physicsTick &&
                    m_renderTick <= (float) m_buffer[i].physicsTick) {
                    itemA = &m_buffer[i + 1];// older data point
                    itemB = &m_buffer[i];    // newer
                    break;
                }
            }

            if (!itemA || !itemB) {
                return (m_renderTick < (float) m_buffer.front().physicsTick)
                               ? m_buffer.front().value// client is behind, use first item
                               : m_buffer.back().value;// client ahead
            }

            float t = (m_renderTick - (float) itemA->physicsTick) /
                      (float) (itemB->physicsTick - itemA->physicsTick);

            t = moe::Math::clamp(t, 0.0f, 1.0f);

            return T::interpolate(itemA->value, itemB->value, t);
        }
    };
}// namespace game