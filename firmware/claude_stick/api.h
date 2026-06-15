#pragma once
#include <stdint.h>

// Uso de rate-limit do Claude (unified), extraído dos headers da resposta.
// Headers validados contra a conta real (ver sondagem): todos os unified-*.
struct UsageData {
    float    h5;                 // utilização 5h em % (0–100)
    float    d7;                 // utilização 7d em % (0–100)
    uint32_t h5ResetEpoch;       // unix ts do reset da janela 5h
    uint32_t d7ResetEpoch;       // unix ts do reset da janela 7d
    uint32_t unifiedResetEpoch;  // unix ts do reset da janela representativa

    char     statusOverall[16];  // allowed | allowed_warning | rejected
    char     status5h[16];       // allowed | rejected | ...
    char     status7d[16];
    char     repClaim[20];       // five_hour | seven_day (quem é o gargalo)
    float    fallbackPct;        // 0–100 (fallback-percentage * 100)
    char     overageStatus[16];  // allowed | rejected
    char     overageReason[32];  // out_of_credits | org_level_disabled | ...

    bool     ok;                 // true se o fetch teve sucesso
    char     error[64];          // mensagem de erro se ok=false
};

bool fetchUsage(const char* token, UsageData& out);
