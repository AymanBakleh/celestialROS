// Test tracking modes implementation
#include <iostream>

// Configuration constants from config.h
#define TRACKING_SIDEREAL  1
#define TRACKING_SOLAR    2
#define TRACKING_LUNAR    0
#define SIDEREAL_DAY  86164.0905f
#define SOLAR_DAY    86400.0f
#define LUNAR_DAY    89428.2f

void test_tracking_rates() {
    std::cout << "=== Tracking Rate Calculations ===" << std::endl;
    
    // Test each tracking mode
    int modes[] = {TRACKING_SIDEREAL, TRACKING_SOLAR, TRACKING_LUNAR};
    const char* mode_names[] = {"Sidereal", "Solar", "Lunar"};
    float day_lengths[] = {SIDEREAL_DAY, SOLAR_DAY, LUNAR_DAY};
    
    for (int i = 0; i < 3; i++) {
        float rate = 360.0f / day_lengths[i];  // degrees per second
        float arcsec_per_sec = rate * 3600.0f;  // Convert to arcseconds
        
        std::cout << mode_names[i] << " Tracking:" << std::endl;
        std::cout << "  Day length: " << day_lengths[i] << " seconds" << std::endl;
        std::cout << "  Rate: " << rate << " deg/sec" << std::endl;
        std::cout << "  Rate: " << arcsec_per_sec << " arcsec/sec" << std::endl;
        std::cout << "  Rate: " << (rate * 86400.0f) << " deg/day" << std::endl;
        std::cout << std::endl;
    }
}

void test_tracking_commands() {
    std::cout << "=== Tracking Command Examples ===" << std::endl;
    
    std::cout << "Set Sidereal Tracking:" << std::endl;
    std::cout << "{\"T\":14,\"mode\":" << TRACKING_SIDEREAL << "}" << std::endl;
    
    std::cout << std::endl << "Set Solar Tracking:" << std::endl;
    std::cout << "{\"T\":14,\"mode\":" << TRACKING_SOLAR << "}" << std::endl;
    
    std::cout << std::endl << "Set Lunar Tracking:" << std::endl;
    std::cout << "{\"T\":14,\"mode\":" << TRACKING_LUNAR << "}" << std::endl;
    
    std::cout << std::endl << "Get Tracking Status:" << std::endl;
    std::cout << "{\"T\":15}" << std::endl;
}

void test_tracking_differences() {
    std::cout << "=== Tracking Rate Differences ===" << std::endl;
    
    float sidereal_rate = 360.0f / SIDEREAL_DAY;
    float solar_rate = 360.0f / SOLAR_DAY;
    float lunar_rate = 360.0f / LUNAR_DAY;
    
    std::cout << "Sidereal vs Solar:" << std::endl;
    std::cout << "  Difference: " << (solar_rate - sidereal_rate) * 3600.0f << " arcsec/sec" << std::endl;
    std::cout << "  Solar is " << ((solar_rate / sidereal_rate - 1.0f) * 100.0f) << "% faster" << std::endl;
    
    std::cout << std::endl << "Sidereal vs Lunar:" << std::endl;
    std::cout << "  Difference: " << (lunar_rate - sidereal_rate) * 3600.0f << " arcsec/sec" << std::endl;
    std::cout << "  Lunar is " << ((lunar_rate / sidereal_rate - 1.0f) * 100.0f) << "% slower" << std::endl;
}

int main() {
    std::cout << "Telescope Mount - Tracking Modes Test" << std::endl;
    std::cout << "====================================" << std::endl;
    
    test_tracking_rates();
    test_tracking_differences();
    test_tracking_commands();
    
    std::cout << std::endl << "=== Implementation Notes ===" << std::endl;
    std::cout << "- Sidereal: Tracks stars (23h 56m 4.0905s day)" << std::endl;
    std::cout << "- Solar: Tracks sun (24h day)" << std::endl;
    std::cout << "- Lunar: Tracks moon (24h 50m 28.2s day)" << std::endl;
    std::cout << "- Commands: T14=set mode, T15=get status" << std::endl;
    
    return 0;
}
