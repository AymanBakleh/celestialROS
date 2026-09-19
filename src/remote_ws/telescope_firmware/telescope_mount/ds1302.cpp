#include "ds1302.h"
#include <Arduino.h>

// DS1302 register addresses
static const uint8_t DS1302_SECONDS = 0x80;
static const uint8_t DS1302_MINUTES = 0x82;
static const uint8_t DS1302_HOURS = 0x84;
static const uint8_t DS1302_DATE = 0x86;
static const uint8_t DS1302_MONTH = 0x88;
static const uint8_t DS1302_DAY = 0x8A;
static const uint8_t DS1302_YEAR = 0x8C;
static const uint8_t DS1302_WP = 0x8E;
static const uint8_t DS1302_BURST = 0xBE;
static const uint8_t DS1302_CLOCK_BURST = 0xBF;

Ds1302::Ds1302(uint8_t ce_pin, uint8_t sclk_pin, uint8_t io_pin) {
    _ce_pin = ce_pin;
    _sclk_pin = sclk_pin;
    _io_pin = io_pin;
    
    pinMode(_ce_pin, OUTPUT);
    pinMode(_sclk_pin, OUTPUT);
    pinMode(_io_pin, OUTPUT);
    
    digitalWrite(_ce_pin, LOW);
    digitalWrite(_sclk_pin, LOW);
}

void Ds1302::init() {
    // Disable write protect
    _writeRegister(DS1302_WP, 0);
}

bool Ds1302::isHalted() {
    uint8_t seconds = _readRegister(DS1302_SECONDS);
    return (seconds & 0x80) != 0;
}

void Ds1302::getDateTime(DateTime* dt) {
    uint8_t burst[8];
    _burstRead(burst);
    
    dt->second = (burst[0] & 0x0F) + ((burst[0] >> 4) & 0x07) * 10;
    dt->minute = (burst[1] & 0x0F) + ((burst[1] >> 4) & 0x07) * 10;
    dt->hour = (burst[2] & 0x0F) + ((burst[2] >> 4) & 0x03) * 10;
    dt->day = (burst[3] & 0x0F) + ((burst[3] >> 4) & 0x03) * 10;
    dt->month = (burst[4] & 0x0F) + ((burst[4] >> 4) & 0x01) * 10;
    dt->year = (burst[5] & 0x0F) + ((burst[5] >> 4) & 0x0F) * 10;
    dt->dow = burst[6] & 0x07;
}

void Ds1302::setDateTime(const DateTime* dt) {
    uint8_t burst[8];
    
    // Convert decimal to BCD
    burst[0] = ((dt->second / 10) << 4) | (dt->second % 10);
    burst[1] = ((dt->minute / 10) << 4) | (dt->minute % 10);
    burst[2] = ((dt->hour / 10) << 4) | (dt->hour % 10);
    burst[3] = ((dt->day / 10) << 4) | (dt->day % 10);
    burst[4] = ((dt->month / 10) << 4) | (dt->month % 10);
    burst[5] = ((dt->year / 10) << 4) | (dt->year % 10);
    burst[6] = dt->dow & 0x07;
    burst[7] = 0; // Write protect flag will be set to 0
    
    // Clear halt bit
    burst[0] &= 0x7F;
    
    _burstWrite(burst);
}

uint8_t Ds1302::_readRegister(uint8_t addr) {
    uint8_t data;
    
    digitalWrite(_ce_pin, HIGH);
    delayMicroseconds(1);
    
    // Send read command
    shiftOut(_io_pin, _sclk_pin, LSBFIRST, addr | 0x01);
    
    // Read data
    pinMode(_io_pin, INPUT);
    data = shiftIn(_io_pin, _sclk_pin, LSBFIRST);
    pinMode(_io_pin, OUTPUT);
    
    digitalWrite(_ce_pin, LOW);
    
    return data;
}

void Ds1302::_writeRegister(uint8_t addr, uint8_t data) {
    digitalWrite(_ce_pin, HIGH);
    delayMicroseconds(1);
    
    // Send write command
    shiftOut(_io_pin, _sclk_pin, LSBFIRST, addr);
    
    // Send data
    shiftOut(_io_pin, _sclk_pin, LSBFIRST, data);
    
    digitalWrite(_ce_pin, LOW);
}

void Ds1302::_burstRead(uint8_t* data) {
    digitalWrite(_ce_pin, HIGH);
    delayMicroseconds(1);
    
    // Send burst read command
    shiftOut(_io_pin, _sclk_pin, LSBFIRST, DS1302_CLOCK_BURST);
    
    // Read 8 bytes
    pinMode(_io_pin, INPUT);
    for (int i = 0; i < 8; i++) {
        data[i] = shiftIn(_io_pin, _sclk_pin, LSBFIRST);
    }
    pinMode(_io_pin, OUTPUT);
    
    digitalWrite(_ce_pin, LOW);
}

void Ds1302::_burstWrite(uint8_t* data) {
    digitalWrite(_ce_pin, HIGH);
    delayMicroseconds(1);
    
    // Send burst write command
    shiftOut(_io_pin, _sclk_pin, LSBFIRST, DS1302_BURST);
    
    // Write 8 bytes
    for (int i = 0; i < 8; i++) {
        shiftOut(_io_pin, _sclk_pin, LSBFIRST, data[i]);
    }
    
    digitalWrite(_ce_pin, LOW);
}
