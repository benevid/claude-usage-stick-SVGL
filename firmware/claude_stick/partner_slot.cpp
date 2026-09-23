#include "partner_slot.h"

// Ver partner_slot.h. O bloco fica em .rodata (DROM, mapeado da flash) e vai
// inteiro para o .bin, zeros inclusive — e isso que o patch pos-build preenche.
// aligned(16): o offset no .bin fica alinhado, o que simplifica o patch.
const PartnerSlot g_partnerSlot __attribute__((aligned(16))) = {
    PARTNER_MAGIC, 0, 0, {0}, {0}
};
