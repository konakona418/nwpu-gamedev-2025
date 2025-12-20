#include "State/DebugToolState.hpp"
#include "App.hpp"
#include "GameManager.hpp"

#include "Param.hpp"

#include "imgui.h"

namespace game::State {
    void DebugToolState::onEnter(GameManager& ctx) {
        ctx.input().get()->addKeyEventMapping("toggle_debug_console", GLFW_KEY_GRAVE_ACCENT);
    }

    void DebugToolState::onExit(GameManager& ctx) {
        ctx.input().get()->removeKeyEventMapping("toggle_debug_console");
    }

    void DebugToolState::onUpdate(GameManager& ctx, float deltaTime) {
        auto input = ctx.input();
        if (!input) {
            return;
        }

        if (input->isKeyJustPressed("toggle_debug_console")) {
            m_showDebugWindow = !m_showDebugWindow;
            moe::Logger::info("Toggling debug window {}", m_showDebugWindow ? "ON" : "OFF");
            if (input->isMouseFree() != m_showDebugWindow) {
                input->setMouseState(m_showDebugWindow);
            }
        }

        if (!m_showDebugWindow) {
            return;
        }

        ctx.renderer().addImGuiDrawCommand([]() {
            auto params = ParamManager::getInstance().getAllParams();
            ImGui::Begin("Debug Tool - Parameters");

            if (ImGui::BeginTable("Params", 2, ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                for (auto& [name_, param]: params) {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(name_.c_str());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-1);

                    auto name = "## " + name_;
                    switch (param->getType()) {
                        case ParamType::Int: {
                            auto value = param->get<ParamInt>();
                            int32_t temp = value;
                            if (ImGui::InputInt(name.c_str(), &temp)) {
                                param->value = temp;
                            }
                            break;
                        }
                        case ParamType::Float: {
                            auto value = param->get<ParamFloat>();
                            if (ImGui::InputFloat(name.c_str(), &value)) {
                                param->value = value;
                            }
                            break;
                        }
                        case ParamType::Bool: {
                            auto value = param->get<ParamBool>();
                            if (ImGui::Checkbox(name.c_str(), &value)) {
                                param->value = value;
                            }
                            break;
                        }
                        case ParamType::String: {
                            auto value = param->get<ParamString>();
                            char buffer[256];
                            std::strncpy(buffer, value.c_str(), sizeof(buffer));
                            if (ImGui::InputText(name.c_str(), buffer, sizeof(buffer))) {
                                param->value = ParamString(buffer);
                            }
                            break;
                        }
                    }
                }

                ImGui::EndTable();
            }

            ImGui::End();
        });
    }
}// namespace game::State