#include "State/DebugToolState.hpp"
#include "App.hpp"
#include "GameManager.hpp"

#include "Param.hpp"

#include "imgui.h"

namespace game::State {
    void DebugToolState::onEnter(GameManager& ctx) {
        ctx.input().addProxy(&m_inputProxy);
        ctx.input().addKeyEventMapping("toggle_debug_console", GLFW_KEY_GRAVE_ACCENT);
        m_inputProxy.setActive(false);// by default proxies are active, disable it initially
    }

    void DebugToolState::onExit(GameManager& ctx) {
        ctx.input().removeKeyEventMapping("toggle_debug_console");
        ctx.input().removeProxy(&m_inputProxy);
    }

    void drawParameterItems(const moe::UnorderedMap<moe::String, ParamItem*>& params, const moe::Vector<moe::String>& sortedNames) {
        if (params.empty()) {
            ImGui::TextUnformatted("No parameters available.");
            return;
        }

        if (ImGui::BeginTable("Params", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            for (auto& name_: sortedNames) {
                auto param = params.at(name_.data());
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
    }

    void DebugToolState::onUpdate(GameManager& ctx, float deltaTime) {
        if (ctx.input().unmanaged().isKeyJustPressed("toggle_debug_console")) {
            m_showDebugWindow = !m_showDebugWindow;
            m_inputProxy.setActive(m_showDebugWindow);// activate/deactivate mouse state handling
            moe::Logger::info("Toggling debug window {}", m_showDebugWindow ? "ON" : "OFF");
        }

        m_inputProxy.setMouseState(m_showDebugWindow);// toggle mouse state based on debug window state

        if (!m_showDebugWindow) {
            return;
        }

        ctx.renderer().addImGuiDrawCommand([]() {
            ImGui::Begin("Debug Tool - Parameters");

            auto& systemParams = ParamManager::getInstance();
            auto& userParams = UserConfigParamManager::getInstance();
            ImGui::TextUnformatted("System Parameters:");
            drawParameterItems(systemParams.getAllParams(), systemParams.getSortedParamNames());
            ImGui::Separator();
            ImGui::TextUnformatted("User Config Parameters:");
            drawParameterItems(userParams.getAllParams(), userParams.getSortedParamNames());

            ImGui::End();
        });
    }
}// namespace game::State