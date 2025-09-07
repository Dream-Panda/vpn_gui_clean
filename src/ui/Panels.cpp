#include "Panels.h"
#include <imgui.h>

void UiPanels::DrawVpnControls(bool running, std::function<void()> onStart, std::function<void()> onStop) {
    ImGui::Begin("VPN");
    ImGui::Text("Status: %s", running ? "Running" : "Stopped");
    if (!running) {
        if (ImGui::Button("Start OpenVPN")) { if (onStart) onStart(); }
    }
    else {
        if (ImGui::Button("Stop OpenVPN")) { if (onStop) onStop(); }
    }
    ImGui::End();
}

void UiPanels::DrawLogs(LogBuffer& buf) {
    ImGui::Begin("Logs");
    ImGui::BeginChild("logscroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::scoped_lock lk(buf.mtx);
        for (auto& l : buf.lines) ImGui::TextUnformatted(l.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}
