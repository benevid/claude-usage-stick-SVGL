// Model health from the Claude status page. Unresolved incidents name the
// affected model family in their text ("Elevated errors on Claude Opus 4.6"),
// so a keyword scan is all the parsing needed — there are no per-model
// components on the status page.
#ifdef BOARD_TDISPLAY_S3

#include "status.h"
#include "config.h"
#include "certs.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

bool fetchModelStatus(ModelStatus& out) {
    WiFiClientSecure client;
    client.setCACert(CA_BUNDLE);

    HTTPClient https;
    if (!https.begin(client, STATUS_ENDPOINT)) {
        Serial.println("[STATUS] https_init failed");
        return false;
    }

    https.addHeader("User-Agent", "claude-usage-stick/1.0");
    https.setTimeout(API_TIMEOUT_MS);
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    Serial.printf("[STATUS] GET %s\n", STATUS_ENDPOINT);
    int code = https.GET();
    Serial.printf("[STATUS] HTTP %d\n", code);

    if (code != 200) {
        https.end();
        return false;   // keep last-known state
    }

    // No PSRAM on this env — a pathological incident feed is treated as a
    // failed fetch rather than risking the heap.
    int len = https.getSize();
    if (len > 65536) {
        Serial.printf("[STATUS] body too large (%d)\n", len);
        https.end();
        return false;
    }

    String body = https.getString();
    https.end();

#ifdef STATUS_TEST_DOWN
    body += " " STATUS_TEST_DOWN;
#endif

    body.toLowerCase();
    out.haikuUp  = body.indexOf("haiku")  < 0;
    out.sonnetUp = body.indexOf("sonnet") < 0;
    out.opusUp   = body.indexOf("opus")   < 0;
    out.fableUp  = body.indexOf("fable")  < 0;
    out.ok = true;

    Serial.printf("[STATUS] haiku:%d sonnet:%d opus:%d fable:%d\n",
                  out.haikuUp, out.sonnetUp, out.opusUp, out.fableUp);
    return true;
}

#endif // BOARD_TDISPLAY_S3
