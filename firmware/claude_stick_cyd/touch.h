#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"

// Driver de toque XPT2046 (SPI separado do display). Mesma interface
// pública do AXS15231B_Touch original (begin/touched/readData).
class CYD_Touch {
public:
    bool begin() {
        _spi.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
        _ts = XPT2046_Touchscreen(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
        return _ts.begin(_spi);
    }

    bool touched() { return _ts.touched(); }

    void readData(uint16_t *x, uint16_t *y) {
        TS_Point p = _ts.getPoint();
        // raw_x/raw_y já saem alinhados com a tela em TFT_ROTATION=1 — sem
        // swap/inversão de eixo (diferente do driver AXS15231B original).
        long sx = constrain(map(p.x, TS_MINX, TS_MAXX, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
        long sy = constrain(map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);
        *x = (uint16_t)sx;
        *y = (uint16_t)sy;
    }

private:
    SPIClass _spi{HSPI};
    XPT2046_Touchscreen _ts{TOUCH_CS_PIN, TOUCH_IRQ_PIN};
};

#endif // TOUCH_H
