#include "Panels.h"
#include "imgui.h"

// ---------- class methods ----------
void UiPanels::DrawUI() {
    ImGui::Begin("VPN");
    ImGui::TextUnformatted("VPN GUI (minimal)");
    ImGui::End();
}

void UiPanels::DrawVpnControls(bool connected, std::function<void()> onStart, std::function<void()> onStop) {
    ImGui::Begin("Controls");
    if (!connected) {
        if (ImGui::Button("Start VPN") && onStart) onStart();
    }
    else {
        if (ImGui::Button("Stop VPN") && onStop) onStop();
    }
    ImGui::End();
}

void UiPanels::DrawLogs(LogBuffer& log) {
    ImGui::Begin("Logs");
    for (const auto& s : log.lines) ImGui::TextUnformatted(s.c_str());
    ImGui::End();
}

// ---------- free-function wrappers (供 main.cpp 直接调用) ----------
static UiPanels g_ui_singleton;

void DrawUI() {
    g_ui_singleton.DrawUI();
}

void DrawVpnControls(bool connected, std::function<void()> onStart, std::function<void()> onStop) {
    g_ui_singleton.DrawVpnControls(connected, onStart, onStop);
}

void DrawLogs(LogBuffer& log) {
    g_ui_singleton.DrawLogs(log);
}
