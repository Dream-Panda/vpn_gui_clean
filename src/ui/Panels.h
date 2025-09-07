#pragma once
#include <functional>
#include <string>
#include <vector>

// --------- simple ring log ----------
struct LogBuffer {
    std::vector<std::string> lines;
    void add(const std::string& s) { lines.push_back(s); if (lines.size() > 2000) lines.erase(lines.begin()); }
    // ���ݾ��÷�
    void clear() { lines.clear(); }
    void push(const std::string& s) { add(s); }
};

// --------- class API (�ڲ�ʵ��) ----------
class UiPanels {
public:
    void DrawUI();
    void DrawVpnControls(bool connected, std::function<void()> onStart, std::function<void()> onStop);
    void DrawLogs(LogBuffer& log);
};

// --------- free functions (main.cpp ���ڵ��õ�����ȫ�ֺ���) ----------
void DrawUI();
void DrawVpnControls(bool connected, std::function<void()> onStart, std::function<void()> onStop);
void DrawLogs(LogBuffer& log);
