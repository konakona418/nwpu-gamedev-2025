#pragma once

#include "Core/Input.hpp"
#include "Core/Meta/Feature.hpp"
#include "Core/RefCounted.hpp"

#include "Math/Common.hpp"

namespace game {
    struct App;
    struct Input;
    struct InputProxy;

    struct MouseButtonState {
        bool pressedLMB;
        bool pressedRMB;
        bool pressedMMB;
    };

#define INPUT_PROXY_FUNCS()                                      \
    bool isKeyPressed(const moe::StringView keyName) const;      \
    bool isKeyJustPressed(const moe::StringView keyName) const;  \
    bool isKeyJustReleased(const moe::StringView keyName) const; \
    moe::Pair<float, float> getMouseDelta() const;               \
    glm::vec2 getMousePosition() const;                          \
    MouseButtonState getMouseButtonState() const;                \
                                                                 \
    void setMouseState(bool isFree);                             \
    bool isMouseFree() const;

    struct UnmanagedInputProxy {
    public:
        explicit UnmanagedInputProxy(Input* input) : m_input(input) {}

        INPUT_PROXY_FUNCS()

    private:
        Input* m_input{nullptr};
    };

    struct Input : moe::Meta::NonCopyable<Input> {
    public:
        friend struct game::App;
        friend struct InputProxy;
        friend struct UnmanagedInputProxy;

        Input(App* app);

        void update();

        void addKeyMapping(const moe::StringView keyName, int keyCode);
        void removeKeyMapping(const moe::StringView keyName);

        void addKeyEventMapping(const moe::StringView keyName, int keyCode);
        void removeKeyEventMapping(const moe::StringView keyName);

        void addProxy(InputProxy* proxy);
        void removeProxy(InputProxy* proxy);

        UnmanagedInputProxy unmanaged() { return UnmanagedInputProxy(this); }

    private:
        // prevent misuse
        INPUT_PROXY_FUNCS()

        struct KeyMap : moe::AtomicRefCounted<KeyMap> {
            int keyCode;
            bool pressed;
        };

        struct KeyEventMap : moe::AtomicRefCounted<KeyEventMap> {
            int keyCode;
            bool justPressed;
            bool justReleased;
        };

        moe::UnorderedMap<int, moe::Ref<KeyMap>> m_keyMap;
        moe::UnorderedMap<moe::String, moe::Ref<KeyMap>> m_keyNameMap;

        moe::UnorderedMap<int, moe::Ref<KeyEventMap>> m_keyEventMap;
        moe::UnorderedMap<moe::String, moe::Ref<KeyEventMap>> m_keyEventNameMap;

        moe::Pair<float, float> m_mouseDelta;

        moe::InputBus* m_inputBus;
        App* m_app;

        moe::Vector<moe::WindowEvent> m_fallThroughEvents;

        moe::UnorderedSet<InputProxy*> m_inputProxies;

        bool m_lastMouseFreeState{true};
        bool m_lastMouseFreeStateValid{false};

        void dispatchKeyDown(int keyCode);
        void dispatchKeyUp(int keyCode);
        void resetKeyEvents();
        void updateProxyStates();
        void updateMouseState();

        // ! fixme: optimize this function to avoid repeated sorting
        moe::Vector<InputProxy*> findMaxPriorityActiveProxies();
    };

    struct InputProxy {
    public:
        friend struct Input;

        static constexpr int PRIORITY_DEFAULT = 0;
        static constexpr int PRIORITY_UI_LOCK = 64;
        static constexpr int PRIORITY_SYSTEM = std::numeric_limits<int>::max();

        explicit InputProxy(int priority = PRIORITY_DEFAULT) : m_priority(priority) {};
        InputProxy(int priority, Input* input) : m_input(input), m_priority(priority) {};

        INPUT_PROXY_FUNCS()

        void setActive(bool active) { m_active = active; }
        bool isActive() const { return m_active; }

        bool isValid() const { return m_isValid; }

    private:
        Input* m_input{nullptr};
        int m_priority{0};

        // this only affects mouse state
        bool m_active{true};
        bool m_mouseFree{false};

        // this is set by Input
        bool m_isValid{true};
    };

#undef INPUT_PROXY_FUNCS
}// namespace game