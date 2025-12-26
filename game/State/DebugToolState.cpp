#include "State/DebugToolState.hpp"
#include "App.hpp"
#include "GameManager.hpp"

#include "InputUtil.hpp"
#include "Localization.hpp"
#include "Math/Util.hpp"
#include "Param.hpp"


#include "imgui.h"

namespace game::State {
    static ParamF IM3D_CAMERA_MOVE_SPEED("debug_tool.im3d_camera_move_speed", 0.1f, ParamScope::UserConfig);

    static void drawParameterItems(const moe::UnorderedMap<moe::String, ParamItem*>& params, const moe::Vector<moe::String>& sortedNames) {
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
                    case ParamType::Float4: {
                        auto value = param->get<ParamFloat4>();
                        float vec[4] = {value.x, value.y, value.z, value.w};
                        if (ImGui::InputFloat4(name.c_str(), vec)) {
                            ParamFloat4 newValue;
                            newValue = {vec[0], vec[1], vec[2], vec[3]};
                            param->value = newValue;
                        }

                        break;
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    static void drawI18NItems(const moe::UnorderedMap<moe::String, LocalizationItem*>& items, const moe::Vector<moe::String>& sortedKeys) {
        if (items.empty()) {
            ImGui::TextUnformatted("No localization items available.");
            return;
        }

        if (ImGui::BeginTable("I18N_Items", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            for (auto& key_: sortedKeys) {
                auto item = items.at(key_.data());
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(key_.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1);

                auto name = "## " + key_;
                auto value = utf8::utf32to8(item->value);
                char buffer[512];
                std::strncpy(buffer, value.c_str(), sizeof(buffer));
                if (ImGui::InputText(name.c_str(), buffer, sizeof(buffer))) {
                    item->value = utf8::utf8to32(moe::StringView(buffer));
                }
            }

            ImGui::EndTable();
        }
    }

    static void drawStateTree(const GameState* state) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        const auto& children = state->getChildStates();
        if (children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        bool isOpen = ImGui::TreeNodeEx((void*) state, flags, "%s", state->getName().data());
        if (isOpen && !children.empty()) {
            for (auto& child: children) {
                drawStateTree(child.get());
            }
            ImGui::TreePop();
        }
    }

    static void drawIm3dGizmo(GameManager& ctx, int gizmoHeight, int gizmoSize, glm::vec3& gizmoTranslation) {
        auto& renderer = ctx.renderer();

        renderer.addIm3dDrawCommand([gizmoHeight, gizmoSize, &gizmoTranslation]() {
            Im3d::PushDrawState();
            Im3d::GetContext().m_gizmoHeightPixels = gizmoHeight;
            Im3d::GetContext().m_gizmoSizePixels = gizmoSize;
            if (Im3d::Gizmo("gizmo",
                            const_cast<float*>(glm::value_ptr(gizmoTranslation)),
                            nullptr, nullptr)) {
            }
            Im3d::PopDrawState();
        });
    }

    void DebugToolState::im3dMovementController(GameManager& ctx) {
        static glm::vec3 debugCameraPos = glm::vec3{0.0f, 0.0f, 0.0f};
        static float cameraYaw = 0.0f;
        static float cameraPitch = 0.0f;

        auto& cam = ctx.renderer().getDefaultCamera();

        bool lmbPressed = m_inputProxy.getMouseButtonState().pressedLMB;
        bool lShiftPressed = m_inputProxy.isKeyPressed("debug_console_im3d_lcontrol");

        if (!m_im3dMovementLocked) {
            if (lmbPressed && lShiftPressed) {
                auto [mouseX, mouseY] = m_inputProxy.getMouseDelta();

                float baseMoveSpeed = IM3D_CAMERA_MOVE_SPEED.get();
                float distFromCamToGizmo = glm::length(debugCameraPos - m_gizmoTranslation);

                distFromCamToGizmo = moe::Math::clamp(distFromCamToGizmo, 0.1f, 1.0f);

                float moveSpeed = baseMoveSpeed * distFromCamToGizmo;
                glm::vec3 right = cam.getRight();
                glm::vec3 realUp = glm::cross(right, cam.getFront());

                glm::vec3 translationDelta = -right * mouseX * moveSpeed + realUp * mouseY * moveSpeed;

                debugCameraPos = debugCameraPos + translationDelta;
            } else if (lmbPressed) {
                auto [mouseX, mouseY] = m_inputProxy.getMouseDelta();

                constexpr float rotateSpeed = 0.3f;

                cameraYaw += mouseX * rotateSpeed;
                cameraPitch += -mouseY * rotateSpeed;

                // prevent gimbal lock
                cameraPitch = moe::Math::clamp(cameraPitch, -89.0f, 89.0f);
            }
        }

        // no matter what, always update camera position
        cam.setPosition(debugCameraPos);
        cam.setYaw(cameraYaw);
        cam.setPitch(cameraPitch);
    }

    static void drawStats(GameManager& ctx) {
        static moe::Deque<App::Stats> historyStats;
        static moe::Deque<moe::PhysicsEngine::Stats> physicsHistoryStats;

        auto appStats = ctx.app().getStats();
        auto physicsStats = ctx.physics().getStats();

        historyStats.push_back(appStats);
        if (historyStats.size() > 100) {
            historyStats.pop_front();
        }

        physicsHistoryStats.push_back(physicsStats);
        if (physicsHistoryStats.size() > 100) {
            physicsHistoryStats.pop_front();
        }

        float avgFrameTime = 0.0f;
        for (auto& stats: historyStats) {
            avgFrameTime += stats.frameTimeMs;
        }
        avgFrameTime /= static_cast<float>(historyStats.size());

        float avgFps = 0.0f;
        for (auto& stats: historyStats) {
            avgFps += stats.fps;
        }
        avgFps /= static_cast<float>(historyStats.size());

        float avgPhysicsFrameTime = 0.0f;
        for (auto& stats: physicsHistoryStats) {
            avgPhysicsFrameTime += stats.physicsFrameTime;
        }
        avgPhysicsFrameTime /= static_cast<float>(physicsHistoryStats.size());

        float avgPhysicsTPS = 0.0f;
        for (auto& stats: physicsHistoryStats) {
            avgPhysicsTPS += stats.physicsTicksPerSecond;
        }
        avgPhysicsTPS /= static_cast<float>(physicsHistoryStats.size());

        ImGui::Begin("Debug Tool - Stats");
        ImGui::TextUnformatted("Graphic Stats:");
        ImGui::PlotLines(
                "Frame Time (ms)",
                [](void* data, int idx) -> float {
                    auto& stats = *static_cast<moe::Deque<App::Stats>*>(data);
                    return stats[idx].frameTimeMs;
                },
                &historyStats,
                static_cast<int>(historyStats.size()));
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                           "Avg Frame Time: %.2f ms", avgFrameTime);

        ImGui::PlotLines(
                "FPS",
                [](void* data, int idx) -> float {
                    auto& stats = *static_cast<moe::Deque<App::Stats>*>(data);
                    return stats[idx].fps;
                },
                &historyStats,
                static_cast<int>(historyStats.size()));
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                           "Avg FPS: %.2f", avgFps);

        ImGui::PlotLines(
                "Physics Frame Time (ms)",
                [](void* data, int idx) -> float {
                    auto& stats = *static_cast<moe::Deque<moe::PhysicsEngine::Stats>*>(data);
                    return stats[idx].physicsFrameTime;
                },
                &physicsHistoryStats,
                static_cast<int>(physicsHistoryStats.size()));
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                           "Avg Physics Frame Time: %.2f ms", avgPhysicsFrameTime);

        ImGui::PlotLines(
                "Physics TPS",
                [](void* data, int idx) -> float {
                    auto& stats = *static_cast<moe::Deque<moe::PhysicsEngine::Stats>*>(data);
                    return stats[idx].physicsTicksPerSecond;
                },
                &physicsHistoryStats,
                static_cast<int>(physicsHistoryStats.size()));
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                           "Avg Physics TPS: %.2f", avgPhysicsTPS);

        ImGui::End();
    }

    void DebugToolState::onEnter(GameManager& ctx) {
        ctx.input().addProxy(&m_inputProxy);
        ctx.input().addKeyEventMapping("toggle_debug_console", GLFW_KEY_GRAVE_ACCENT);
        ctx.input().addKeyMapping("debug_console_im3d_lcontrol", GLFW_KEY_LEFT_CONTROL);

        m_inputProxy.setActive(false);// by default proxies are active, disable it initially

        ctx.addDebugDrawFunction(
                "Parameters",
                [this, &ctx]() {
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

        ctx.addDebugDrawFunction(
                "Localization",
                [this, &ctx]() {
                    ImGui::Begin("Debug Tool - Localization");
                    auto& i18nManager = LocalizationParamManager::getInstance();
                    drawI18NItems(i18nManager.getAllLocalizationItems(), i18nManager.getSortedKeys());
                    ImGui::End();
                });

        ctx.addDebugDrawFunction(
                "Game State Tree",
                [this, &ctx]() {
                    ImGui::Begin("Debug Tool - Game State Tree");
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "From top to bottom");
                    ImGui::TextUnformatted("Current State Stack:");
                    auto currentStack = ctx.getCurrentStateStack();
                    for (auto it = currentStack.rbegin(); it != currentStack.rend(); ++it) {
                        drawStateTree(it->get());
                    }
                    ImGui::Separator();
                    ImGui::TextUnformatted("Persistent State Stack:");
                    auto persistentStack = ctx.getPersistentStateStack();
                    for (auto it = persistentStack.rbegin(); it != persistentStack.rend(); ++it) {
                        drawStateTree(it->get());
                    }
                    ImGui::End();
                });

        ctx.addDebugDrawFunction(
                "Im3d Gizmo",
                [this, &ctx]() {
                    static int gizmoHeight = 50;
                    static int gizmoSize = 5;

                    ImGui::Begin("Debug Tool - Im3d Gizmo");
                    if (ImGui::BeginTable("GizmoSettings", 2)) {

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Im3d Gizmo Height");
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::InputInt("##gizmo_height", &gizmoHeight);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Im3d Gizmo Size");
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::InputInt("##gizmo_size", &gizmoSize);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Lock Im3d Cam");
                        ImGui::TableNextColumn();
                        ImGui::Checkbox("##lock_im3d_cam", &m_im3dMovementLocked);

                        ImGui::EndTable();
                    }

                    ImGui::Button("Reset Gizmo Pos");
                    if (ImGui::IsItemClicked()) {
                        m_gizmoTranslation = glm::vec3{0.0f, 0.0f, 0.0f};
                    }

                    ImGui::Text("Gizmo position: (%f, %f, %f)",
                                m_gizmoTranslation.x, m_gizmoTranslation.y, m_gizmoTranslation.z);

                    ImGui::End();

                    drawIm3dGizmo(ctx, gizmoHeight, gizmoSize, m_gizmoTranslation);
                    im3dMovementController(ctx);
                });

        ctx.addDebugDrawFunction(
                "Stats",
                [this, &ctx]() {
                    drawStats(ctx);
                });
    }

    void DebugToolState::onExit(GameManager& ctx) {
        ctx.input().removeKeyEventMapping("toggle_debug_console");
        ctx.input().removeKeyMapping("debug_console_im3d_lcontrol");
        ctx.input().removeProxy(&m_inputProxy);

        ctx.removeDebugDrawFunction("Parameters");
        ctx.removeDebugDrawFunction("Localization");
        ctx.removeDebugDrawFunction("Game State Tree");
        ctx.removeDebugDrawFunction("Im3d Gizmo");
        ctx.removeDebugDrawFunction("Stats");
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

        ctx.renderer()
                .addImGuiDrawCommand([this, &ctx]() {
                    ImGui::Begin("Debug Tool");
                    ImGui::TextUnformatted("Debug Draw Functions:");
                    for (auto& [name, debugDrawFunction]: ctx.m_debugDrawFunctions) {
                        if (ImGui::Checkbox(name.c_str(), &debugDrawFunction.isActive)) {
                            moe::Logger::debug(
                                    "DebugToolState: set debug draw function '{}' active state to {}",
                                    name,
                                    debugDrawFunction.isActive ? "true" : "false");
                        }
                    }
                    ImGui::End();

                    for (auto& [name, debugDrawFunction]: ctx.m_debugDrawFunctions) {
                        if (debugDrawFunction.isActive) {
                            debugDrawFunction.drawFunction();
                        }
                    }
                });
    }
}// namespace game::State