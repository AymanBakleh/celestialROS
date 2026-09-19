#ifndef DS1302_H
#define DS1302_H

#include <Arduino.h>

class Ds1302 {
public:
    // DateTime structure exactly like your examples
    struct DateTime {
        uint8_t year;    // 00-99 (years since 2000)
        uint8_t month;   // 01-12
        uint8_t day;     // 01-31
        uint8_t hour;    // 00-23
        uint8_t minute;  // 00-59
        uint8_t second;  // 00-59
        uint8_t dow;     // 01-07 (1=Monday, 7=Sunday)
    };
    
    // Day of week constants exactly like your examples
    static const uint8_t DOW_MON = 1;
    static const uint8_t DOW_TUE = 2;
    static const uint8_t DOW_WED = 3;
    static const uint8_t DOW_THU = 4;
    static const uint8_t DOW_FRI = 5;
    static const uint8_t DOW_SAT = 6;
    static const uint8_t DOW_SUN = 7;
    
    // Month constants exactly like your examples
    static const uint8_t MONTH_JAN = 1;
    static const uint8_t MONTH_FEB = 2;
    static const uint8_t MONTH_MAR = 3;
    static const uint8_t MONTH_APR = 4;
    static const uint8_t MONTH_MAY = 5;
    static const uint8_t MONTH_JUN = 6;
    static const uint8_t MONTH_JUL = 7;
    static const uint8_t MONTH_AUG = 8;
    static const uint8_t MONTH_SEP = 9;
    static const uint8_t MONTH_OCT = 10;
    static const uint8_t MONTH_NOV = 11;
    static const uint8_t MONTH_DEC = 12;
    
    // Constructor exactly like your examples: Ds1302(ce, sclk, io)
    Ds1302(uint8_t ce_pin, uint8_t sclk_pin, uint8_t io_pin);
    
    // Initialize the RTC exactly like your examples
    void init();
    
    // Check if RTC is halted exactly like your examples
    bool isHalted();
    
    // Get date and time exactly like your examples
    void getDateTime(DateTime* dt);
    
    // Set date and time exactly like your examples
    void setDateTime(const DateTime* dt);
    
private:
    uint8_t _ce_pin;
    uint8_t _sclk_pin;
    uint8_t _io_pin;
    
    // Internal helper functions
    uint8_t _readRegister(uint8_t addr);
    void _writeRegister(uint8_t addr, uint8_t data);
    void _burstRead(uint8_t* data);
    void _burstWrite(uint8_t* data);
};

#endif // DS1302_H
