#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/Feature.hpp"

namespace moe {
    struct WindowEvent {
        struct Close {};
        struct Minimize {};
        struct RestoreFromMinimize {};
        struct Resize {
            uint32_t width;
            uint32_t height;
        };

        // ! keydown keyup is not safe. some events will randomly be lost
        struct KeyDown {
            uint32_t keyCode;
        };

        struct KeyRepeat {
            uint32_t keyCode;
        };

        struct KeyUp {
            uint32_t keyCode;
        };

        struct MouseMove {
            float deltaX;
            float deltaY;
        };

        Variant<std::monostate,
                Close,
                Minimize,
                RestoreFromMinimize,
                Resize,
                KeyDown,
                KeyRepeat,
                KeyUp,
                MouseMove>
                args;

        template<typename T>
        bool is() { return std::holds_alternative<T>(args); }

        template<typename T>
        auto getIf() {
            if (std::holds_alternative<T>(args)) {
                return Optional<T>{std::get<T>(args)};
            }
            return Optional<T>{std::nullopt};
        }
    };

    struct InputBus : public Meta::NonCopyable<InputBus> {
        friend class VulkanEngine;

        void pushEvent(WindowEvent event) {
            m_pollingEvents.push_back(event);
        }

        Optional<WindowEvent> peekEvent() {
            if (m_pollingEvents.empty()) {
                return {};
            }
            return Optional<WindowEvent>{m_pollingEvents.front()};
        }

        Optional<WindowEvent> pollEvent() {
            if (m_pollingEvents.empty()) {
                return {};
            }
            auto ret = m_pollingEvents.front();
            m_pollingEvents.pop_front();
            return Optional<WindowEvent>{ret};
        }

        bool isKeyPressed(int key) const {
            MOE_ASSERT(m_isKeyPressedFunc != nullptr, "isKeyPressed function not set");
            return m_isKeyPressedFunc(key);
        }

        void setMouseValid(bool valid) {
            MOE_ASSERT(m_setMouseValidFunc != nullptr, "setMouseValid function not set");
            m_setMouseValidFunc(valid);
        }

        void* getNativeHandle() const {
            MOE_ASSERT(m_getNativeHandleFunc != nullptr, "getNativeHandle function not set");
            return m_getNativeHandleFunc();
        }

    private:
        Deque<WindowEvent> m_pollingEvents;
        Function<bool(int)> m_isKeyPressedFunc{nullptr};
        Function<void(bool)> m_setMouseValidFunc{nullptr};
        Function<void*()> m_getNativeHandleFunc{nullptr};
    };
}// namespace moe