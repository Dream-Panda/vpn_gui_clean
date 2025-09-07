#include "Panels.h"
#include "imgui.h"

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
    for (auto& s : log.lines) ImGui::TextUnformatted(s.c_str());
    ImGui::End();
}
