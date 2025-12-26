#pragma once

#include "GameManager.hpp"

#include "Core/Meta/TypeTraits.hpp"

namespace game {
    // naive FSM implementation
    // ! fixme: implement is_enum_v
    template<typename StateE, StateE InitialState = {}, typename = moe::Meta::EnableIfT<std::is_enum_v<StateE>>>
    struct SimpleFSM {
    public:
        using StateFactoryFn = moe::Function<void(SimpleFSM<StateE>&, float)>;

        SimpleFSM() = default;
        explicit SimpleFSM(GameManager& ctx) { m_ctx = &ctx; }

        void setContext(GameManager& ctx) { m_ctx = &ctx; }

        StateE getCurrentStateEnum() const {
            return m_currentStateEnum;
        }

        GameManager& getContext() {
            MOE_ASSERT(m_ctx != nullptr, "SimpleFSM: context is null");
            return *m_ctx;
        }

        void transitionTo(StateE newState) {
            if (m_currentStateEnum == newState) {
                return;
            }

            if (auto it = m_stateFactories.find(newState); it != m_stateFactories.end()) {
                m_currentStateEnum = newState;
                m_currentState = it->second;
            } else {
                moe::Logger::error("SimpleFSM::transitionTo: no factory registered for state {}", static_cast<int>(newState));
            }
        }

        void state(
                StateE state,
                StateFactoryFn factory) {
            m_stateFactories[state] =
                    std::make_shared<StateFactoryFn>(std::move(factory));
        }

        void update(float deltaTime) {
            if (!m_currentState) {
                if (auto it = m_stateFactories.find(InitialState); it != m_stateFactories.end()) {
                    m_currentStateEnum = InitialState;
                    m_currentState = it->second;
                } else {
                    moe::Logger::error("SimpleFSM::update: no factory registered for initial state {}", static_cast<int>(InitialState));
                    return;
                }
            }
            (*m_currentState)(*this, deltaTime);
        }

    private:
        GameManager* m_ctx{nullptr};

        StateE m_currentStateEnum = InitialState;

        moe::SharedPtr<StateFactoryFn> m_currentState{nullptr};
        moe::UnorderedMap<StateE, moe::SharedPtr<StateFactoryFn>> m_stateFactories{};
    };

}// namespace game