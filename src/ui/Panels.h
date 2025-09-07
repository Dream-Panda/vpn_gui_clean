// src/ui/Panels.h  ���� ����
#pragma once
#include <functional>
#include <string>
#include <vector>

struct LogBuffer {
    std::vector<std::string> lines;
    void add(const std::string& s) { lines.push_back(s); if (lines.size() > 2000) lines.erase(lines.begin()); }
    // ������ main.cpp ����÷�
    void clear() { lines.clear(); }
    void push(const std::string& s) { add(s); }
};

class UiPanels {
public:
    void DrawUI();
    void DrawVpnControls(bool connected, std::function<void()> onStart, std::function<void()> onStop);
    void DrawLogs(LogBuffer& log);
};
