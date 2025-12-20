#include "App.hpp"

#include "State/LocalPlayerState.hpp"
#include "State/PauseUIState.hpp"
#include "State/PlaygroundState.hpp"
#include "State/WorldEnvironment.hpp"

#include "Param.hpp"

namespace game {
    void App::init() {
        moe::Logger::setThreadName("Graphics");

        moe::MainScheduler::getInstance().init();
        moe::ThreadPoolScheduler::getInstance().init();
        moe::FileReader::initReader(new moe::DebugFileReader<moe::DefaultFileReader>());

        ParamManager::getInstance().loadFromFile(moe::userdata("config.toml"));

        m_physicsEngine = &moe::PhysicsEngine::getInstance();
        m_physicsEngine->init();

        m_gameManager = std::make_unique<game::GameManager>(this);

        // persist the game manager's physics update function on the physics thread
        m_physicsEngine->persistOnPhysicsThread([this](moe::PhysicsEngine& engine) {
            auto dt = moe::PhysicsEngine::PHYSICS_TIMESTEP.count();
            this->m_gameManager->physicsUpdate(dt);
        });

        m_audioEngine = std::make_unique<moe::AudioEngineInterface>(moe::AudioEngine::getInstance().getInterface());

        m_graphicsEngine = std::make_unique<moe::VulkanEngine>();
        m_graphicsEngine->init();

        m_input = std::make_unique<Input>(this);
    }

    void App::shutdown() {
        m_graphicsEngine->cleanup();
        m_physicsEngine->destroy();

        ParamManager::getInstance().saveToFile(moe::userdata("config.toml"));

        moe::ThreadPoolScheduler::getInstance().shutdown();
        moe::MainScheduler::getInstance().shutdown();
        moe::FileReader::destroyReader();
    }

    void App::run() {
        bool running = true;
        auto lastTime = std::chrono::high_resolution_clock::now();

        auto worldEnv = moe::Ref(new State::WorldEnvironment());
        m_gameManager->pushState(worldEnv);

        auto pgstate = moe::Ref(new State::PlaygroundState());
        m_gameManager->pushState(pgstate);

        auto playerState = moe::Ref(new State::LocalPlayerState());
        m_gameManager->pushState(playerState);

        while (running) {
            m_graphicsEngine->beginFrame();

            // reset key events, remove unused events
            m_input->update();
            for (auto& e: m_input->m_fallThroughEvents) {
                if (auto closeEvent = e.getIf<moe::WindowEvent::Close>()) {
                    running = false;
                } else if (auto keyDownEvent = e.getIf<moe::WindowEvent::KeyDown>()) {
                    m_input->dispatchKeyDown(keyDownEvent->keyCode);
                } else if (auto keyUpEvent = e.getIf<moe::WindowEvent::KeyUp>()) {
                    m_input->dispatchKeyUp(keyUpEvent->keyCode);
                }
            }

            auto now = std::chrono::high_resolution_clock::now();
            auto deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(now - lastTime).count();
            lastTime = now;

            moe::MainScheduler::getInstance().processTasks();

            // switch read buffer
            m_physicsEngine->updateReadBuffer();
            m_gameManager->update(deltaTime);

            m_graphicsEngine->endFrame();
        }
    }

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

    moe::Pair<float, float> Input::getMousePosition() const {
        auto* window = static_cast<GLFWwindow*>(m_inputBus->getNativeHandle());
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        return moe::Pair<float, float>{static_cast<float>(x), static_cast<float>(y)};
    }

    Input::MouseButtonState Input::getMouseButtonState(int button) const {
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

    void Input::setMouseState(bool isFree) {
        m_inputBus->setMouseValid(isFree);
    }

    void Input::update() {
        resetKeyEvents();
        m_fallThroughEvents.clear();
        m_mouseDelta = std::make_pair(0, 0);
        while (auto e = m_inputBus->pollEvent()) {
            if (auto mouseEvent = e->getIf<moe::WindowEvent::MouseMove>()) {
                m_mouseDelta = std::make_pair(mouseEvent->deltaX, mouseEvent->deltaY);
            }

            m_fallThroughEvents.push_back(e.value());
        }

        for (auto& [keyCode, keyMap]: m_keyMap) {
            keyMap->pressed = m_inputBus->isKeyPressed(keyCode);
        }

        // ! todo: handle mouse events
    }
}// namespace game