#ifndef PARTNER_SLOT_H
#define PARTNER_SLOT_H

#include <stdint.h>

// Slot de logo de parceiro, gravado no binario DEPOIS da compilacao.
//
// O firmware e compilado uma unica vez com este bloco constante (magic + w=h=0)
// na flash (.rodata). O tools/partner_logo.py acha o magic dentro do .bin,
// escreve w, h e os pixels e recalcula checksum + SHA-256 do app image — sem
// recompilar por cliente. w == 0 significa "sem logo": o header mostra o
// wordmark de sempre.
//
// A definicao fica em partner_slot.cpp, de proposito: o .ino so ve o `extern`,
// entao o compilador nao consegue dobrar `w == 0` em constante (o valor real
// so existe depois do patch). Leia w/h pelas funcoes abaixo, que passam por
// volatile pelo mesmo motivo.
#define PARTNER_MAX_W 170
#define PARTNER_MAX_H 36
#define PARTNER_MAGIC "USAGESTICK-LOGO"      // 15 chars + NUL = 16 bytes

struct __attribute__((packed)) PartnerSlot {
    char     magic[16];
    uint16_t w, h;                            // 0,0 = slot vazio
    uint8_t  reserved[12];                    // cabecalho fecha em 32 bytes
    uint8_t  px[PARTNER_MAX_W * PARTNER_MAX_H * 4];   // ARGB8888, bytes B,G,R,A
};

extern const PartnerSlot g_partnerSlot;

inline uint16_t partnerLogoW() { return *(const volatile uint16_t *)&g_partnerSlot.w; }
inline uint16_t partnerLogoH() { return *(const volatile uint16_t *)&g_partnerSlot.h; }
inline bool partnerLogoPresent() {
    uint16_t w = partnerLogoW(), h = partnerLogoH();
    return w > 0 && w <= PARTNER_MAX_W && h > 0 && h <= PARTNER_MAX_H;
}

#endif // PARTNER_SLOT_H
