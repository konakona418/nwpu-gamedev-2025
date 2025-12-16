#pragma once

#include "Audio/AudioCommand.hpp"
#include "Audio/Common.hpp"


#include "Core/Common.hpp"
#include "Math/Common.hpp"


MOE_BEGIN_NAMESPACE

struct AudioEngine;

struct AudioListener {
public:
    void init();
    void destroy();

    void setPosition(const glm::vec3& pos);
    void setVelocity(const glm::vec3& vel);
    void setOrientation(const glm::vec3& forward, const glm::vec3& up = Math::VK_GLOBAL_UP);
    void setGain(float gain);
};

struct SetListenerPositionCommand : public AudioCommand {
    glm::vec3 position;

    explicit SetListenerPositionCommand(const glm::vec3& pos)
        : position(pos) {}

    void execute(AudioEngine& engine) override;
};

struct SetListenerVelocityCommand : public AudioCommand {
    glm::vec3 velocity;

    explicit SetListenerVelocityCommand(const glm::vec3& vel)
        : velocity(vel) {}

    void execute(AudioEngine& engine) override;
};

struct SetListenerOrientationCommand : public AudioCommand {
    glm::vec3 forward;
    glm::vec3 up;

    SetListenerOrientationCommand(const glm::vec3& fwd, const glm::vec3& upVec)
        : forward(fwd), up(upVec) {}

    void execute(AudioEngine& engine) override;
};

struct SetListenerGainCommand : public AudioCommand {
    float gain;

    explicit SetListenerGainCommand(float g)
        : gain(g) {}

    void execute(AudioEngine& engine) override;
};

MOE_END_NAMESPACE
