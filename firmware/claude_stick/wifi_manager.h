#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"

#define MAX_SAVED_NETWORKS 3

class WiFiManager {
public:
    struct NetworkInfo {
        char ssid[33];
        int rssi;
        bool open;
    };

    bool begin() {
        _prefs.begin("wifi", false);
        _loadAll();
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        return true;
    }

    // Tenta cada rede salva até uma conectar
    bool autoConnect(int timeout_ms = 10000) {
        for (int i = 0; i < _count; i++) {
            Serial.printf("WiFi: trying '%s' (%d/%d)...\n", _nets[i].ssid, i + 1, _count);
            WiFi.begin(_nets[i].ssid, _nets[i].pass);

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < (unsigned long)timeout_ms) {
                delay(100);
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("WiFi: connected to '%s'! IP=%s\n",
                    _nets[i].ssid, WiFi.localIP().toString().c_str());
                if (i > 0) _promote(i);
                return true;
            }
            WiFi.disconnect();
        }
        Serial.println("WiFi: no saved network available");
        return false;
    }

    bool connectTo(const char *ssid, const char *pass, int timeout_ms = 15000) {
        Serial.printf("WiFi: connecting to '%s'...\n", ssid);
        WiFi.begin(ssid, pass);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < (unsigned long)timeout_ms) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("WiFi: connected! IP=%s\n", WiFi.localIP().toString().c_str());
            _addNetwork(ssid, pass);
            return true;
        }
        Serial.println("WiFi: connection failed");
        WiFi.disconnect();
        return false;
    }

    int scanNetworks(NetworkInfo *results, int max_results) {
        int n = WiFi.scanNetworks();
        int count = min(n, max_results);
        for (int i = 0; i < count; i++) {
            strncpy(results[i].ssid, WiFi.SSID(i).c_str(), 32);
            results[i].ssid[32] = '\0';
            results[i].rssi = WiFi.RSSI(i);
            results[i].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        }
        WiFi.scanDelete();
        return count;
    }

    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    String getIP() { return WiFi.localIP().toString(); }
    String getSSID() { return WiFi.SSID(); }

    String getSavedSSID() {
        if (_count > 0) return String(_nets[0].ssid);
        return "";
    }

    int getSavedCount() { return _count; }
    const char *getSavedSSID(int idx) {
        if (idx >= 0 && idx < _count) return _nets[idx].ssid;
        return "";
    }

    void disconnect() { WiFi.disconnect(); }

    bool connectSaved(int idx, int timeout_ms = 10000) {
        if (idx < 0 || idx >= _count) return false;
        return connectTo(_nets[idx].ssid, _nets[idx].pass, timeout_ms);
    }

    void forgetNetwork(int idx) {
        if (idx < 0 || idx >= _count) return;
        for (int i = idx; i < _count - 1; i++) _nets[i] = _nets[i + 1];
        _count--;
        _saveAll();
        Serial.printf("WiFi: forgot network at index %d, %d remaining\n", idx, _count);
    }

    void forgetAll() {
        _count = 0;
        _prefs.clear();
        Serial.println("WiFi: all networks forgotten");
    }

private:
    Preferences _prefs;

    struct SavedNet {
        char ssid[33];
        char pass[65];
    };
    SavedNet _nets[MAX_SAVED_NETWORKS];
    int _count = 0;

    void _loadAll() {
        _count = _prefs.getInt("count", 0);
        if (_count > MAX_SAVED_NETWORKS) _count = MAX_SAVED_NETWORKS;
        for (int i = 0; i < _count; i++) {
            char ks[8], kp[8];
            snprintf(ks, sizeof(ks), "s%d", i);
            snprintf(kp, sizeof(kp), "p%d", i);
            String s = _prefs.getString(ks, "");
            String p = _prefs.getString(kp, "");
            strncpy(_nets[i].ssid, s.c_str(), 32); _nets[i].ssid[32] = '\0';
            strncpy(_nets[i].pass, p.c_str(), 64); _nets[i].pass[64] = '\0';
        }
    }

    void _saveAll() {
        _prefs.putInt("count", _count);
        for (int i = 0; i < _count; i++) {
            char ks[8], kp[8];
            snprintf(ks, sizeof(ks), "s%d", i);
            snprintf(kp, sizeof(kp), "p%d", i);
            _prefs.putString(ks, _nets[i].ssid);
            _prefs.putString(kp, _nets[i].pass);
        }
    }

    void _addNetwork(const char *ssid, const char *pass) {
        for (int i = 0; i < _count; i++) {
            if (strcmp(_nets[i].ssid, ssid) == 0) {
                strncpy(_nets[i].pass, pass, 64);
                if (i > 0) _promote(i);
                _saveAll();
                return;
            }
        }
        int slots = min(_count + 1, MAX_SAVED_NETWORKS);
        for (int i = slots - 1; i > 0; i--) {
            _nets[i] = _nets[i - 1];
        }
        strncpy(_nets[0].ssid, ssid, 32); _nets[0].ssid[32] = '\0';
        strncpy(_nets[0].pass, pass, 64); _nets[0].pass[64] = '\0';
        _count = slots;
        _saveAll();
    }

    void _promote(int idx) {
        if (idx <= 0 || idx >= _count) return;
        SavedNet tmp = _nets[idx];
        for (int i = idx; i > 0; i--) _nets[i] = _nets[i - 1];
        _nets[0] = tmp;
        _saveAll();
    }
};

extern WiFiManager g_wifi;

#endif // WIFI_MANAGER_H
