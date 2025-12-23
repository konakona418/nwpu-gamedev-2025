#include "Input.hpp"
#include "App.hpp"

namespace game {
    Input::Input(App* app) : m_app(app) {
        m_inputBus = &app->m_graphicsEngine->getInputBus();
    }

    void Input::addKeyMapping(moe::StringView keyName, int keyCode) {
        if (m_keyNameMap.find(moe::String(keyName)) != m_keyNameMap.end()) {
            moe::Logger::warn("Input::addKeyMapping: key mapping for '{}' already exists", keyName);
        }

        auto keyMap = moe::Ref(new KeyMap());
        keyMap->pressed = false;

        m_keyMap[keyCode] = keyMap;
        m_keyNameMap[moe::String(keyName)] = keyMap;
    }

    void Input::removeKeyMapping(moe::StringView keyName) {
        auto it = m_keyNameMap.find(moe::String(keyName));
        if (it != m_keyNameMap.end()) {
            // ! in case of duplicate key codes, we do not erase from m_keyMap here
            m_keyNameMap.erase(it);
        }
    }

    void Input::addKeyEventMapping(const moe::StringView keyName, int keyCode) {
        if (m_keyEventNameMap.find(moe::String(keyName)) != m_keyEventNameMap.end()) {
            moe::Logger::warn("Input::addKeyEventMapping: key event mapping for '{}' already exists", keyName);
        }

        auto keyEventMap = moe::Ref(new KeyEventMap());
        keyEventMap->justPressed = false;
        keyEventMap->justReleased = false;

        m_keyEventMap[keyCode] = keyEventMap;
        m_keyEventNameMap[moe::String(keyName)] = keyEventMap;
    }

    void Input::removeKeyEventMapping(const moe::StringView keyName) {
        auto it = m_keyEventNameMap.find(moe::String(keyName));
        if (it != m_keyEventNameMap.end()) {
            m_keyEventNameMap.erase(it);
        }
    }

    void Input::dispatchKeyDown(int keyCode) {
        auto it = m_keyEventMap.find(keyCode);
        if (it != m_keyEventMap.end()) {
            it->second->justPressed = true;
        }
    }

    void Input::dispatchKeyUp(int keyCode) {
        auto it = m_keyEventMap.find(keyCode);
        if (it != m_keyEventMap.end()) {
            it->second->justReleased = true;
        }
    }

    void Input::resetKeyEvents() {
        for (auto& [keyCode, keyEventMap]: m_keyEventMap) {
            keyEventMap->justPressed = false;
            keyEventMap->justReleased = false;
        }
    }

    moe::Vector<InputProxy*> Input::findMaxPriorityActiveProxies() {
        moe::Vector<InputProxy*> result;
        if (m_inputProxies.empty()) {
            return result;
        }

        moe::Vector<InputProxy*> activeProxies;
        for (auto* proxy: m_inputProxies) {
            if (proxy->m_active) {
                activeProxies.push_back(proxy);
            }
        }

        if (activeProxies.empty()) {
            return result;
        }

        std::sort(activeProxies.begin(), activeProxies.end(),
                  [](InputProxy* a, InputProxy* b) {
                      return a->m_priority > b->m_priority;
                  });
        int maxPriority = activeProxies.front()->m_priority;
        for (auto* proxy: activeProxies) {
            if (proxy->m_priority == maxPriority) {
                result.push_back(proxy);
            }
        }

        return result;
    }

    void Input::updateProxyStates() {
        if (m_inputProxies.empty()) {
            return;
        }

        auto activeProxies = findMaxPriorityActiveProxies();
        for (auto* proxy: m_inputProxies) {
            proxy->m_isValid = false;
        }
        for (auto* proxy: activeProxies) {
            proxy->m_isValid = true;
        }
    }

    void Input::updateMouseState() {
        if (m_inputProxies.empty()) {
            return;
        }

        auto activeProxies = findMaxPriorityActiveProxies();
        bool shouldShowMouse = true;
        for (auto* proxy: activeProxies) {
            if (!proxy->m_mouseFree) {
                shouldShowMouse = false;
                break;
            }
        }

        if (shouldShowMouse == m_lastMouseFreeState && m_lastMouseFreeStateValid) {
            return;
        }

        m_lastMouseFreeState = shouldShowMouse;
        m_lastMouseFreeStateValid = true;

        auto window = static_cast<GLFWwindow*>(m_inputBus->getNativeHandle());
        if (!shouldShowMouse) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            }
        }
    }

    glm::vec2 Input::getMousePosition() const {
        auto* window = static_cast<GLFWwindow*>(m_inputBus->getNativeHandle());
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        return glm::vec2{static_cast<float>(x), static_cast<float>(y)};
    }

    moe::Pair<float, float> Input::getMouseDelta() const {
        return m_mouseDelta;
    }

    MouseButtonState Input::getMouseButtonState() const {
        auto* window = static_cast<GLFWwindow*>(m_inputBus->getNativeHandle());
        MouseButtonState state;
        state.pressedLMB = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        state.pressedRMB = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        state.pressedMMB = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        return state;
    }

    bool Input::isKeyPressed(moe::StringView keyName) const {
        auto it = m_keyNameMap.find(moe::String(keyName));
        if (it != m_keyNameMap.end()) {
            return it->second->pressed;
        }

        return false;
    }

    bool Input::isKeyJustPressed(const moe::StringView keyName) const {
        auto it = m_keyEventNameMap.find(moe::String(keyName));
        if (it != m_keyEventNameMap.end()) {
            return it->second->justPressed;
        }

        return false;
    }

    bool Input::isKeyJustReleased(const moe::StringView keyName) const {
        auto it = m_keyEventNameMap.find(moe::String(keyName));
        if (it != m_keyEventNameMap.end()) {
            return it->second->justReleased;
        }

        return false;
    }

    bool Input::isMouseFree() const {
        auto window = static_cast<GLFWwindow*>(m_inputBus->getNativeHandle());
        auto mode = glfwGetInputMode(window, GLFW_CURSOR);
        return mode == GLFW_CURSOR_NORMAL;
    }

    void Input::addProxy(InputProxy* proxy) {
        proxy->m_input = this;
        m_inputProxies.insert(proxy);
    }

    void Input::removeProxy(InputProxy* proxy) {
        proxy->m_input = nullptr;
        m_inputProxies.erase(proxy);
    }

    void Input::update() {
        resetKeyEvents();
        m_fallThroughEvents.clear();
        m_mouseDelta = moe::Pair<float, float>(0.f, 0.f);
        while (auto e = m_inputBus->pollEvent()) {
            if (auto mouseEvent = e->getIf<moe::WindowEvent::MouseMove>()) {
                m_mouseDelta = moe::Pair<float, float>(mouseEvent->deltaX, mouseEvent->deltaY);
            }

            m_fallThroughEvents.push_back(e.value());
        }

        for (auto& [keyCode, keyMap]: m_keyMap) {
            keyMap->pressed = m_inputBus->isKeyPressed(keyCode);
        }

        // proxy state update
        updateProxyStates();

        updateMouseState();
    }

    bool InputProxy::isKeyPressed(const moe::StringView keyName) const {
        if (!m_isValid || !m_input) {
            return false;
        }
        return m_input->isKeyPressed(keyName);
    }

    bool InputProxy::isKeyJustPressed(const moe::StringView keyName) const {
        if (!m_isValid || !m_input) {
            return false;
        }
        return m_input->isKeyJustPressed(keyName);
    }

    bool InputProxy::isKeyJustReleased(const moe::StringView keyName) const {
        if (!m_isValid || !m_input) {
            return false;
        }
        return m_input->isKeyJustReleased(keyName);
    }

    moe::Pair<float, float> InputProxy::getMouseDelta() const {
        if (!m_isValid || !m_input) {
            return moe::Pair<float, float>{0.f, 0.f};
        }
        return m_input->getMouseDelta();
    }

    glm::vec2 InputProxy::getMousePosition() const {
        if (!m_isValid || !m_input) {
            return glm::vec2{0.f, 0.f};
        }
        return m_input->getMousePosition();
    }

    MouseButtonState InputProxy::getMouseButtonState() const {
        if (!m_isValid || !m_input) {
            return MouseButtonState{false, false, false};
        }
        return m_input->getMouseButtonState();
    }

    void InputProxy::setMouseState(bool isFree) {
        if (!m_isValid || !m_input) {
            return;
        }
        m_mouseFree = isFree;
    }

    bool InputProxy::isMouseFree() const {
        // this does not affect mouse state, just query
        return m_input->isMouseFree();
    }

    bool UnmanagedInputProxy::isKeyPressed(const moe::StringView keyName) const {
        return m_input->isKeyPressed(keyName);
    }

    bool UnmanagedInputProxy::isKeyJustPressed(const moe::StringView keyName) const {
        return m_input->isKeyJustPressed(keyName);
    }

    bool UnmanagedInputProxy::isKeyJustReleased(const moe::StringView keyName) const {
        return m_input->isKeyJustReleased(keyName);
    }

    moe::Pair<float, float> UnmanagedInputProxy::getMouseDelta() const {
        return m_input->getMouseDelta();
    }

    glm::vec2 UnmanagedInputProxy::getMousePosition() const {
        return m_input->getMousePosition();
    }

    MouseButtonState UnmanagedInputProxy::getMouseButtonState() const {
        return m_input->getMouseButtonState();
    }
}// namespace game