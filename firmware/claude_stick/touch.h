#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>
#include <Wire.h>

// Driver de toque AXS15231B (versão validada do bring-up/projeto anterior).
#define AXS_GET_POINT_X(buf) (((uint16_t)(buf[2] & 0x0F) << 8) + (uint16_t)buf[3])
#define AXS_GET_POINT_Y(buf) (((uint16_t)(buf[4] & 0x0F) << 8) + (uint16_t)buf[5])

class AXS15231B_Touch {
public:
    AXS15231B_Touch(uint8_t scl, uint8_t sda, uint8_t int_pin, uint8_t addr, uint8_t rotation)
        : _scl(scl), _sda(sda), _int_pin(int_pin), _addr(addr), _rotation(rotation) {}

    bool begin() {
        _instance = this;
        pinMode(_int_pin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(_int_pin), _isr, FALLING);
        return Wire.begin(_sda, _scl, 400000);
    }

    bool touched() { return _update(); }

    void readData(uint16_t *x, uint16_t *y) {
        *x = _point_x;
        *y = _point_y;
    }

    void setRotation(uint8_t r) { _rotation = r; }

private:
    uint8_t _scl, _sda, _int_pin, _addr, _rotation;
    volatile bool _touch_int = false;
    uint16_t _point_x = 0, _point_y = 0;

    static AXS15231B_Touch *_instance;

    static void ARDUINO_ISR_ATTR _isr() {
        if (_instance) _instance->_touch_int = true;
    }

    bool _update() {
        if (!_touch_int) return false;
        _touch_int = false;

        static const uint8_t read_cmd[8] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08};
        uint8_t buf[8] = {0};

        Wire.beginTransmission(_addr);
        Wire.write(read_cmd, sizeof(read_cmd));
        Wire.endTransmission();

        Wire.requestFrom(_addr, (uint8_t)sizeof(buf));
        for (int i = 0; i < (int)sizeof(buf) && Wire.available(); i++)
            buf[i] = Wire.read();

        uint16_t raw_x = AXS_GET_POINT_X(buf);
        uint16_t raw_y = AXS_GET_POINT_Y(buf);

        if (raw_x == 0 && raw_y == 0) return false;

        switch (_rotation) {
            case 0: _point_x = raw_x; _point_y = raw_y; break;
            case 1: _point_x = raw_y; _point_y = 319 - raw_x; break;
            case 2: _point_x = 319 - raw_x; _point_y = 479 - raw_y; break;
            case 3: _point_x = 479 - raw_y; _point_y = raw_x; break;
        }
        return true;
    }
};

AXS15231B_Touch *AXS15231B_Touch::_instance = nullptr;

#endif // TOUCH_H
