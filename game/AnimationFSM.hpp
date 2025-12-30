#pragma once

#include "Core/Common.hpp"
#include "Core/Meta/TypeTraits.hpp"

namespace game {
    struct Animation {
        moe::String name;
        float durationFrames;
    };

    template<typename StateE, typename = moe::Meta::EnableIf<std::is_enum_v<StateE>>>
    struct AnimationFSM {
    public:
        using StateFn = moe::Function<void(AnimationFSM&)>;
        using StateArg = moe::Variant<std::monostate, moe::String, bool, int, float, void*>;

        struct State {
            StateE stateId;
            Animation animation;
            bool loop;
            StateE nextIfNoLoop;

            StateFn onEnter;
            StateFn onExit;
            StateFn onUpdate;
        };

        struct Sample {
            Animation animation;
            float timeInAnimation;

            Sample() : animation{"", 0.0f}, timeInAnimation(0.0f) {}
        };

        explicit AnimationFSM(StateE initialState, float fps = 30.0f)
            : m_currentStateId(initialState), m_fps(fps) {}

        void setFPS(float fps) {
            m_fps = fps;
        }

        float getFPS() const {
            return m_fps;
        }

        void addState(
                StateE stateId,
                const Animation& animation,
                StateFn onUpdate,
                StateFn onEnter = nullptr,
                StateFn onExit = nullptr) {
            State state;

            state.stateId = stateId;
            state.animation = animation;
            state.loop = true;
            state.onEnter = std::move(onEnter);
            state.onExit = std::move(onExit);
            state.onUpdate = std::move(onUpdate);

            m_states[stateId] = std::move(state);
        }

        void addStateNoLoop(
                StateE stateId,
                const Animation& animation,
                StateE next,
                StateFn onUpdate,
                StateFn onEnter = nullptr,
                StateFn onExit = nullptr) {
            State state;

            state.stateId = stateId;
            state.animation = animation;
            state.loop = false;
            state.nextIfNoLoop = next;
            state.onEnter = std::move(onEnter);
            state.onExit = std::move(onExit);
            state.onUpdate = std::move(onUpdate);

            m_states[stateId] = std::move(state);
        }

        void update(float deltaTime) {
            auto it = m_states.find(m_currentStateId);
            if (it == m_states.end()) {
                return;
            }

            State& state = it->second;
            m_timeInCurrentStateSecs += deltaTime;
            if (m_timeInCurrentStateSecs >= state.animation.durationFrames / m_fps) {
                if (!state.loop) {
                    transitionTo(state.nextIfNoLoop);
                    return;
                }
                // animation ended, loop
                m_timeInCurrentStateSecs = 0.0f;
            }

            if (state.onUpdate) {
                state.onUpdate(*this);
            } else {
                moe::Logger::warn(
                        "AnimationFSM::update: no onUpdate function for state {}",
                        static_cast<std::underlying_type_t<StateE>>(m_currentStateId));
            }
        }

        void transitionTo(StateE newStateId) {
            auto itCurrent = m_states.find(m_currentStateId);
            if (itCurrent != m_states.end()) {
                State& currentState = itCurrent->second;
                if (currentState.onExit) {
                    currentState.onExit(*this);
                }
            }

            m_currentStateId = newStateId;
            m_timeInCurrentStateSecs = 0.0f;

            auto itNew = m_states.find(m_currentStateId);
            if (itNew != m_states.end()) {
                State& newState = itNew->second;
                if (newState.onEnter) {
                    newState.onEnter(*this);
                }
            }
        }

        Sample sampleCurrentAnimation() const {
            auto it = m_states.find(m_currentStateId);
            if (it == m_states.end()) {
                moe::Logger::error(
                        "AnimationFSM::sampleCurrentAnimation: invalid current state {}",
                        static_cast<std::underlying_type_t<StateE>>(m_currentStateId));
                return Sample{};
            }

            const State& state = it->second;
            Sample sample;
            sample.animation = state.animation;
            sample.timeInAnimation = m_timeInCurrentStateSecs;
            return sample;
        }

        template<typename T>
        T getStateArg(const moe::String& key, const T& defaultValue = T()) const {
            auto it = m_stateArgs.find(key);
            if (it == m_stateArgs.end()) {
                return defaultValue;
            }

            const StateArg& arg = it->second;
            auto value = std::get_if<T>(&arg);
            if (value) {
                return *value;
            } else {
                return defaultValue;
            }
        }

        void setStateArg(const moe::String& key, StateArg value) {
            m_stateArgs[key] = std::move(value);
        }

    private:
        float m_fps{30.0f};

        moe::UnorderedMap<StateE, State> m_states;
        StateE m_currentStateId;

        float m_timeInCurrentStateSecs{0.0f};

        moe::UnorderedMap<moe::String, StateArg> m_stateArgs;
    };
}// namespace game