// =============================
// Minimal ImGui VPN UI (single-file)
// Target: Windows + VS2022 + GLFW + OpenGL3 + Dear ImGui
// No real VPN logic; purely UI + a tiny state machine using glfwGetTime().
// Drop this into your existing ImGui project as main.cpp (or replace your UI render part).
// =============================

#include "vpn_logic.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

// ---- Tiny helper: theme ----
static void ApplyMinimalTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 12.0f;
    s.FrameRounding = 10.0f;
    s.GrabRounding = 10.0f;
    s.ScrollbarRounding = 12.0f;
    s.TabRounding = 10.0f;
}

// ---- Minimal state machine (no threads) ----
enum class VpnState { Idle, Resolving, Connecting, Connected, Disconnecting, Error };

struct AppState {
    // Profiles
    std::vector<std::string> profiles{ "US-West #1", "US-East #2", "CA-Vancouver", "JP-Tokyo" };
    int selected_profile = 0;

    // Options
    bool auto_reconnect = false;
    int reconnect_interval_sec = 15;

    // Connection state
    VpnState state = VpnState::Idle;
    std::string status_msg = "Idle";
    std::string assigned_ip;

    // Timing for fake progress
    double t_phase_start = 0.0; // glfwGetTime() when phase began

    // Log buffer (very simple)
    ImVector<ImVec4> log_colors; // per line color
    ImVector<ImGuiTextBuffer> logs; // (one buffer per line for simplicity)

    void Log(const char* text, ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f)) {
        ImGuiTextBuffer line; line.appendf("%s\n", text);
        logs.push_back(line);
        log_colors.push_back(color);
    }
};

static const char* StateToText(VpnState s) {
    switch (s) {
    case VpnState::Idle: return "Idle";
    case VpnState::Resolving: return "Resolving";
    case VpnState::Connecting: return "Connecting";
    case VpnState::Connected: return "Connected";
    case VpnState::Disconnecting: return "Disconnecting";
    case VpnState::Error: return "Error";
    }
    return "?";
}

static void StartConnect(AppState& app) {
    if (app.state != VpnState::Idle && app.state != VpnState::Error) return;
    app.state = VpnState::Resolving;
    app.status_msg = "Resolving server...";
    app.assigned_ip.clear();
    app.t_phase_start = glfwGetTime();
    app.Log("Connect clicked");
}

static void StartDisconnect(AppState& app) {
    if (app.state == VpnState::Connected || app.state == VpnState::Connecting || app.state == VpnState::Resolving) {
        app.state = VpnState::Disconnecting;
        app.status_msg = "Disconnecting...";
        app.t_phase_start = glfwGetTime();
        app.Log("Disconnect clicked");
    }
}

static void UpdateFakeState(AppState& app) {
    const double now = glfwGetTime();
    const double dt = now - app.t_phase_start;

    switch (app.state) {
    case VpnState::Resolving:
        if (dt > 0.6) {
            app.state = VpnState::Connecting;
            app.status_msg = "Handshaking...";
            app.t_phase_start = now;
            app.Log("Resolved endpoint", ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
        }
        break;
    case VpnState::Connecting:
        if (dt > 1.0) {
            app.state = VpnState::Connected;
            app.status_msg = "Connected";
            app.assigned_ip = "10.8.0.2"; // fake assigned ip
            app.t_phase_start = now;
            app.Log("Tunnel established", ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
        }
        break;
    case VpnState::Disconnecting:
        if (dt > 0.4) {
            app.state = VpnState::Idle;
            app.status_msg = "Idle";
            app.assigned_ip.clear();
            app.t_phase_start = now;
            app.Log("Disconnected", ImVec4(0.9f, 0.6f, 0.6f, 1.0f));
        }
        break;
    default: break; // Idle/Connected/Error: do nothing
    }
}

// ---- UI drawing ----
static void DrawStatusPill(const char* label, VpnState st) {
    ImVec4 color;
    switch (st) {
    case VpnState::Idle: color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
    case VpnState::Resolving: color = ImVec4(0.7f, 0.7f, 0.2f, 1.0f); break;
    case VpnState::Connecting: color = ImVec4(0.8f, 0.6f, 0.2f, 1.0f); break;
    case VpnState::Connected: color = ImVec4(0.2f, 0.7f, 0.3f, 1.0f); break;
    case VpnState::Disconnecting: color = ImVec4(0.7f, 0.4f, 0.2f, 1.0f); break;
    case VpnState::Error: color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f); break;
    }
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::Button(label);
    ImGui::PopStyleColor(3);
}

static void DrawMainUI(AppState& app) {
    ImGui::Begin("VPN Control", nullptr, ImGuiWindowFlags_NoCollapse);

    // Top row: profile select + connect/disconnect
    ImGui::TextUnformatted("Profile:");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##profile", app.profiles[app.selected_profile].c_str())) {
        for (int i = 0; i < (int)app.profiles.size(); ++i) {
            bool sel = (i == app.selected_profile);
            if (ImGui::Selectable(app.profiles[i].c_str(), sel)) {
                app.selected_profile = i;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (app.state == VpnState::Connected || app.state == VpnState::Connecting || app.state == VpnState::Resolving) {
        if (ImGui::Button("Disconnect", ImVec2(120, 0))) StartDisconnect(app);
    }
    else {
        if (ImGui::Button("Connect", ImVec2(120, 0))) StartConnect(app);
    }

    ImGui::Separator();

    // Status line
    ImGui::Text("Status: "); ImGui::SameLine(); DrawStatusPill(StateToText(app.state), app.state);
    if (!app.assigned_ip.empty()) {
        ImGui::SameLine();
        ImGui::Text("IP: %s", app.assigned_ip.c_str());
    }

    ImGui::TextWrapped("%s", app.status_msg.c_str());

    // Options
    ImGui::Separator();
    ImGui::Checkbox("Auto reconnect", &app.auto_reconnect);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Interval (s)", &app.reconnect_interval_sec);
    if (app.reconnect_interval_sec < 5) app.reconnect_interval_sec = 5;

    // Log panel
    ImGui::Separator();
    ImGui::TextUnformatted("Logs");
    ImGui::BeginChild("logchild", ImVec2(0, 180), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < app.logs.Size; ++i) {
        ImGui::PushStyleColor(ImGuiCol_Text, app.log_colors[i]);
        ImGui::TextUnformatted(app.logs[i].c_str());
        ImGui::PopStyleColor();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    if (ImGui::Button("Clear Logs")) { app.logs.clear(); app.log_colors.clear(); }

    ImGui::End();
}

int main(int, char**) {// test commit from VS
    if (!glfwInit()) return 1;

    // GL + GLSL versions
    const char* glsl_version = "#version 150"; // GL 3.2 core (safe for VS/Windows)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1000, 650, "Minimal VPN UI", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ApplyMinimalTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    AppState app;
    app.Log("App started");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        UpdateFakeState(app);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main UI
        DrawMainUI(app);

        ImGui::Render();
        int display_w, display_h; glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.07f, 0.08f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// =============================
// QUICK START (choose one):
// A) If you already have ImGui+GLFW wired in VS project:
//    - Replace your current main.cpp with this file (or call DrawMainUI(app) from your existing frame loop).
//    - Ensure you link: glfw3dll.lib (or glfw3.lib depending static/dll), opengl32.lib
//    - Ensure backends are compiled: imgui_impl_glfw.cpp, imgui_impl_opengl3.cpp
//
// B) If starting fresh with CMake+vcpkg: (optional, minimal outline)
//    - Enable vcpkg and add packages: glfw3, imgui[glfw-binding,opengl3-binding]
//    - Use OpenGL loader provided by the backend (no glad needed for basic UI)
// =============================
