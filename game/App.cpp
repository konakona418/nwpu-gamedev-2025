#include "App.hpp"

#include "State/DebugToolState.hpp"
#include "State/LocalPlayerState.hpp"
#include "State/PlaygroundState.hpp"
#include "State/WorldEnvironment.hpp"

#include "Input.hpp"
#include "Localization.hpp"
#include "Param.hpp"


#include "Core/FileReader.hpp"
#include "Core/Task/Scheduler.hpp"

#ifndef NDEBUG
#ifndef ENABLE_DEV_MODE
#define ENABLE_DEV_MODE
#endif
#endif

namespace game {
    static ParamI WINDOW_WIDTH("window.width", 1280, ParamScope::UserConfig);
    static ParamI WINDOW_HEIGHT("window.height", 720, ParamScope::UserConfig);

    static ParamF FOV_DEGREES("graphics.fov_degrees", 45.0f, ParamScope::UserConfig);

    static ParamS SERVER_ADDRESS("network.server_address", "127.0.0.1", ParamScope::UserConfig);
    static ParamI SERVER_PORT("network.server_port", 12345, ParamScope::UserConfig);

    static ParamS IMGUI_FONT_PATH("graphics.imgui_font_path", "assets/fonts/NotoSansSC-Regular.ttf", ParamScope::System);

    void App::init() {
        moe::Logger::setThreadName("Graphics");

        moe::MainScheduler::getInstance().init();
        moe::ThreadPoolScheduler::getInstance().init();
        moe::FileReader::initReader(new moe::DebugFileReader<moe::DefaultFileReader>());

        ParamManager::getInstance().loadFromFile(moe::asset("config.toml"));
        LocalizationParamManager::getInstance().loadFromFile(moe::asset("localization.toml"));
#ifdef ENABLE_DEV_MODE
        ParamManager::getInstance().setDevMode(true);
        LocalizationParamManager::getInstance().setDevMode(true);
#else
        ParamManager::getInstance().setDevMode(false);
        LocalizationParamManager::getInstance().setDevMode(false);
#endif
        UserConfigParamManager::getInstance().loadFromFile(moe::userdata("settings.toml"));

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
        m_graphicsEngine->init({
                .viewportWidth = static_cast<int>(WINDOW_WIDTH.get()),
                .viewportHeight = static_cast<int>(WINDOW_HEIGHT.get()),
                .fovDeg = FOV_DEGREES.get(),
                .csmCameraScale = {3.0f, 3.0f, 3.0f},
                .imGuiFontPath = moe::asset(IMGUI_FONT_PATH.get()),
        });

        m_input = std::make_unique<Input>(this);

        m_networkAdaptor = std::make_unique<NetworkAdaptor>();
        m_networkAdaptor->init(SERVER_ADDRESS.get(), static_cast<uint16_t>(SERVER_PORT.get()));
    }

    void App::shutdown() {
        m_networkAdaptor->shutdown();
        m_graphicsEngine->cleanup();
        m_physicsEngine->destroy();

#ifdef ENABLE_DEV_MODE
        ParamManager::getInstance().saveToFile();
        LocalizationParamManager::getInstance().saveToFile();
#endif
        UserConfigParamManager::getInstance().saveToFile();

        moe::ThreadPoolScheduler::getInstance().shutdown();
        moe::MainScheduler::getInstance().shutdown();
        moe::FileReader::destroyReader();
    }

    void App::run() {
        bool running = true;
        auto lastTime = std::chrono::high_resolution_clock::now();

        auto worldEnv = moe::Ref(new State::WorldEnvironment());
        m_gameManager->pushState(worldEnv);

        auto debugToolState = moe::Ref(new State::DebugToolState());
        m_gameManager->pushState(debugToolState);

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
}// namespace game