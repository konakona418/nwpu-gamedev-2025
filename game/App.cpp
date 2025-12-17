#include "App.hpp"

namespace game {
    void App::init() {
        moe::MainScheduler::getInstance().init();
        moe::ThreadPoolScheduler::getInstance().init();
        moe::FileReader::initReader(new moe::DebugFileReader<moe::DefaultFileReader>());

        m_physicsEngine = &moe::PhysicsEngine::getInstance();
        m_physicsEngine->init();

        // persist the game manager's physics update function on the physics thread
        m_physicsEngine->persistOnPhysicsThread([this](moe::PhysicsEngine& engine) {
            auto dt = moe::PhysicsEngine::PHYSICS_TIMESTEP.count();
            this->m_gameManager.physicsUpdate(dt);
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
        while (running) {
            m_graphicsEngine->beginFrame();

            auto inputBus = m_graphicsEngine->getInputBus();
            while (auto e = inputBus.pollEvent()) {
                if (e->is<moe::WindowEvent::Close>()) {
                    running = false;
                }
            }

            auto now = std::chrono::high_resolution_clock::now();
            auto deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(now - lastTime).count();
            lastTime = now;

            moe::MainScheduler::getInstance().processTasks();

            m_gameManager.update(deltaTime);

            m_graphicsEngine->endFrame();
        }
    }
}// namespace game