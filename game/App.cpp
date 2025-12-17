#include "App.hpp"

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

        m_graphicsEngine->getDefaultCamera().setPosition(glm::vec3(0, 2, 0));

        while (running) {
            m_graphicsEngine->beginFrame();

            auto& inputBus = m_graphicsEngine->getInputBus();
            while (auto e = inputBus.pollEvent()) {
                if (e->is<moe::WindowEvent::Close>()) {
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
}// namespace game