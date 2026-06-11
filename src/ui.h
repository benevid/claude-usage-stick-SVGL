#pragma once
#include "api.h"
#ifdef MANGO_UI
#include "status.h"
#endif

void uiInit();
void uiBootProgress(int percent, const char* label);
void uiSetupScreen(const char* apName, const char* apPass);
void uiPinScreen(int pos, const int digits[4]);
void uiConnecting(const char* ssid, int attempt = 0);
void uiDashboard(const UsageData& data, unsigned long lastFetchMs, int rssi, int batPct);
// Lightweight in-place update of the clock + reset countdowns (no bars, no full clear)
// so the periodic refresh doesn't flicker. Call when only time has passed, not data.
void uiDashboardClock(const UsageData& data, unsigned long lastFetchMs, int rssi);
void uiError(const char* title, const char* detail = nullptr);
void uiLockout(int attempts, int maxAttempts, int lockoutSec);
#ifdef MANGO_UI
// Latest model health for the dashboard's mascot row; cached until the next call.
void uiSetModelStatus(const ModelStatus& s);
// Flip the panel 180° (and clear it); caller redraws the current screen.
void uiToggleRotation();
// Close (true) or open (false) the healthy mascots' eyes on the dashboard.
void uiBlinkTick(bool closed);
#endif // MANGO_UI
