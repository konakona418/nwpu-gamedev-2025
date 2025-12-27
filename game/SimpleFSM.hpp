#pragma once

#include "GameManager.hpp"

#include "Core/Meta/TypeTraits.hpp"

namespace game {
    // naive FSM implementation
    // ! fixme: implement is_enum_v
    template<typename StateE, StateE InitialState = {}, typename = moe::Meta::EnableIfT<std::is_enum_v<StateE>>>
    struct SimpleFSM {
    public:
        using StatePreFn = moe::Function<void(SimpleFSM<StateE>&)>;
        using StatePostFn = moe::Function<void(SimpleFSM<StateE>&)>;
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
                // call post function of current state
                if (m_currentState && m_currentState->postFn) {
                    m_currentState->postFn(*this);
                }

                m_currentStateEnum = newState;
                m_currentState = it->second;

                // call pre function of new state
                if (m_currentState->preFn) {
                    m_currentState->preFn(*this);
                }
            } else {
                moe::Logger::error("SimpleFSM::transitionTo: no factory registered for state {}", static_cast<int>(newState));
            }
        }

        void state(
                StateE state,
                StateFactoryFn factory,
                StatePreFn preFn = nullptr,
                StatePostFn postFn = nullptr) {
            m_stateFactories[state] =
                    std::make_shared<State>(State{state, std::move(preFn), std::move(postFn), std::move(factory)});
        }

        void update(float deltaTime) {
            if (!m_currentState) {
                if (auto it = m_stateFactories.find(InitialState); it != m_stateFactories.end()) {
                    m_currentStateEnum = InitialState;
                    m_currentState = it->second;

                    // call pre function of initial state
                    if (m_currentState->preFn) {
                        m_currentState->preFn(*this);
                    }
                } else {
                    moe::Logger::error("SimpleFSM::update: no factory registered for initial state {}", static_cast<int>(InitialState));
                    return;
                }
            }
            m_currentState->factoryFn(*this, deltaTime);
        }

    private:
        GameManager* m_ctx{nullptr};

        struct State {
            StateE stateEnum;
            StatePreFn preFn;
            StatePostFn postFn;
            StateFactoryFn factoryFn;
        };

        StateE m_currentStateEnum = InitialState;

        moe::SharedPtr<State> m_currentState{nullptr};
        moe::UnorderedMap<StateE, moe::SharedPtr<State>> m_stateFactories{};
    };

}// namespace game