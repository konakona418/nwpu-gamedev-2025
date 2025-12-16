#include "Audio/AudioListener.hpp"
#include "Audio/AudioEngine.hpp"

MOE_BEGIN_NAMESPACE

void AudioListener::init() {
    // initialize with default values
    ALfloat listenerPos[] = {0.0f, 0.0f, 0.0f};
    ALfloat listenerVel[] = {0.0f, 0.0f, 0.0f};
    ALfloat listenerOri[] = {0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f};

    alListenerfv(AL_POSITION, listenerPos);
    alListenerfv(AL_VELOCITY, listenerVel);
    alListenerfv(AL_ORIENTATION, listenerOri);
}

void AudioListener::destroy() {
}

void AudioListener::setPosition(const glm::vec3& pos) {
    alListenerfv(AL_POSITION, glm::value_ptr(pos));
}

void AudioListener::setVelocity(const glm::vec3& vel) {
    alListenerfv(AL_VELOCITY, glm::value_ptr(vel));
}

void AudioListener::setOrientation(const glm::vec3& forward, const glm::vec3& up) {
    float orient[6] = {
            forward.x, forward.y, forward.z,
            up.x, up.y, up.z};
    alListenerfv(AL_ORIENTATION, orient);
}

void AudioListener::setGain(float gain) {
    alListenerf(AL_GAIN, gain);
}

void SetListenerPositionCommand::execute(AudioEngine& engine) {
    AudioListener& listener = engine.getListener();
    alListenerfv(AL_POSITION, glm::value_ptr(position));
}

void SetListenerVelocityCommand::execute(AudioEngine& engine) {
    AudioListener& listener = engine.getListener();
    alListenerfv(AL_VELOCITY, glm::value_ptr(velocity));
}

void SetListenerOrientationCommand::execute(AudioEngine& engine) {
    AudioListener& listener = engine.getListener();
    float orient[6] = {
            forward.x, forward.y, forward.z,
            up.x, up.y, up.z};
    alListenerfv(AL_ORIENTATION, orient);
}

void SetListenerGainCommand::execute(AudioEngine& engine) {
    AudioListener& listener = engine.getListener();
    alListenerf(AL_GAIN, gain);
}

MOE_END_NAMESPACE