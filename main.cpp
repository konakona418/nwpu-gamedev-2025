#include "Render/Vulkan/VulkanEngine.hpp"

#include "Audio/AudioEngine.hpp"

#include "Core/FileReader.hpp"
#include "Core/Task/Scheduler.hpp"

int main() {
    moe::MainScheduler::getInstance().init();
    moe::ThreadPoolScheduler::getInstance().init();
    moe::FileReader::initReader(new moe::DebugFileReader<moe::DefaultFileReader>());

    auto& audioEngine = moe::AudioEngine::getInstance();
    auto audioInterface = audioEngine.getInterface();

    moe::VulkanEngine engine;
    engine.init();

    engine.run();

    engine.cleanup();
    moe::ThreadPoolScheduler::shutdown();
    moe::MainScheduler::getInstance().shutdown();
    moe::FileReader::destroyReader();

    return 0;
}