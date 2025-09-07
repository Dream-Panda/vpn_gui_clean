#include "OpenVpnRunner.h"
#include <codecvt>
#include <locale>

static std::wstring widen(const std::string& s) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> cv; return cv.from_bytes(s);
}
static std::string narrow(const std::wstring& w) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> cv; return cv.to_bytes(w);
}

bool OpenVpnRunner::start(const OpenVpnConfig& cfg,
    std::function<void(const std::string&)> onOutput,
    std::function<void(const std::string&)> /*onError*/) {
    ProcessOptions opt;
    opt.exe = cfg.openvpnExe;
    opt.args = { L"--config", cfg.ovpnFile, L"--verb", L"3" };
    for (auto& a : cfg.extraArgs) opt.args.push_back(a);
    opt.hidden = true;

    std::wstring err;
    bool ok = runner_.start(opt, &err);
    if (!ok && onOutput) { onOutput(narrow(L"[OpenVPN] start failed: " + err)); }
    else if (ok && onOutput) { onOutput("[OpenVPN] started"); }
    return ok;
}

void OpenVpnRunner::stop() {
    runner_.stop();
}
