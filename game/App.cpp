#include "App.hpp"

#include "State/LocalPlayerState.hpp"
#include "State/PauseUIState.hpp"
#include "State/PlaygroundState.hpp"
#include "State/WorldEnvironment.hpp"

namespace game {
    void App::init() {
        moe::Logger::setThreadName("Graphics");

        moe::MainScheduler::getInstance().init();
        moe::ThreadPoolScheduler::getInstance().init();
        moe::FileReader::initReader(new moe::DebugFileReader<moe::DefaultFileReader>());

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

        auto pauseState = moe::Ref(new State::PauseUIState());
        m_gameManager->pushState(pauseState);

        while (running) {
            m_graphicsEngine->beginFrame();

            m_input->update();
            for (auto& e: m_input->m_fallThroughEvents) {
                if (auto closeEvent = e.getIf<moe::WindowEvent::Close>()) {
                    running = false;
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
        auto keyMap = moe::Ref(new KeyMap());
        keyMap->pressed = false;

        m_keyMap[keyCode] = keyMap;
        m_keyNameMap[moe::String(keyName)] = keyMap;
    }

    void Input::removeKeyMapping(moe::StringView keyName) {
        auto it = m_keyNameMap.find(moe::String(keyName));
        if (it != m_keyNameMap.end()) {
            m_keyMap.erase(it->second->keyCode);
            m_keyNameMap.erase(it);
        }
    }

    bool Input::isKeyPressed(moe::StringView keyName) const {
        auto it = m_keyNameMap.find(moe::String(keyName));
        if (it != m_keyNameMap.end()) {
            return it->second->pressed;
        }

        return false;
    }

    void Input::setMouseState(bool isFree) {
        m_inputBus->setMouseValid(isFree);
    }

    void Input::update() {
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