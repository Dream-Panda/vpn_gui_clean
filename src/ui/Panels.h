#pragma once
#include <string>
#include <deque>
#include <mutex>

struct LogBuffer {
    std::deque<std::string> lines;
    std::mutex mtx;
    void push(const std::string& s) {
        std::scoped_lock lk(mtx);
        lines.push_back(s);
        if (lines.size() > 2000) lines.pop_front();
    }
    void clear() {
        std::scoped_lock lk(mtx);
        lines.clear();
    }
};

namespace UiPanels {
    void DrawVpnControls(bool running, std::function<void()> onStart, std::function<void()> onStop);
    void DrawLogs(LogBuffer& buf);
}
