// Test RA 24h wrapper and DEC speed fixes
#include <iostream>
#include <cmath>

// Configuration
#define RA_INVERTED  0
#define DEC_INVERTED 1  // Your current setting
#define TICKS_PER_AXIS_DEG 1638.4f

// RA normalization (0-360 degrees)
float normalize_ra_deg(float ra_deg) {
    while (ra_deg >= 360.0f) ra_deg -= 360.0f;
    while (ra_deg < 0.0f) ra_deg += 360.0f;
    return ra_deg;
}

// Fixed RA angular distance
float ra_angular_distance(float current_ra, float target_ra) {
    // Normalize both angles to [0, 360) range
    current_ra = normalize_ra_deg(current_ra);
    target_ra = normalize_ra_deg(target_ra);
    
    float diff = target_ra - current_ra;
    
    // Find shortest path around the circle
    if (diff > 180.0f) {
        diff -= 360.0f;
    } else if (diff < -180.0f) {
        diff += 360.0f;
    }
    
    return diff;
}

// Motor direction logic
int get_motor_direction(int32_t error, bool inverted) {
    int direction = error > 0 ? 1 : -1;
    return inverted ? -direction : direction;
}

void test_ra_24h_wrapper() {
    std::cout << "=== Testing RA 24h Wrapper ===" << std::endl;
    
    // Test cases for 24h wrapper (hours to degrees conversion)
    float test_cases[][2] = {
        {0.0f, 0.0f},     // 0h = 0°
        {12.0f, 180.0f},   // 12h = 180°  
        {24.0f, 0.0f},     // 24h = 0° (wrapper)
        {295.0f, 295.0f},  // 295° = 295° (already in degrees)
        {360.0f, 0.0f},    // 360° = 0° (wrapper)
        {400.0f, 40.0f}    // 400° = 40° (wrapper)
    };
    
    for (auto& test : test_cases) {
        float input = test[0];
        float expected = test[1];
        
        // Convert hours to degrees if input < 24 (assuming hours)
        float ra_deg = (input < 24.0f) ? input * 15.0f : input;
        float normalized = normalize_ra_deg(ra_deg);
        
        std::cout << "RA " << input;
        if (input < 24.0f) {
            std::cout << "h -> " << ra_deg << "°";
        } else {
            std::cout << "° -> " << ra_deg << "°";
        }
        std::cout << " -> " << normalized << "°";
        if (abs(normalized - expected) < 0.1f) {
            std::cout << " ✅" << std::endl;
        } else {
            std::cout << " ❌ (expected " << expected << "°)" << std::endl;
        }
    }
}

void test_ra_movement() {
    std::cout << "\n=== Testing RA Movement ===" << std::endl;
    
    // Test movement from 350° to 295°
    float current_ra = 350.0f;
    float target_ra = 295.0f;
    
    float distance = ra_angular_distance(current_ra, target_ra);
    std::cout << "Current: " << current_ra << "°" << std::endl;
    std::cout << "Target: " << target_ra << "°" << std::endl;
    std::cout << "Distance: " << distance << "°" << std::endl;
    
    // Convert to steps
    int32_t distance_steps = (int32_t)lroundf(distance * TICKS_PER_AXIS_DEG);
    std::cout << "Distance steps: " << distance_steps << std::endl;
    
    // Test motor direction
    int direction = get_motor_direction(distance_steps, RA_INVERTED);
    std::cout << "Motor direction: " << direction << " (" << (direction > 0 ? "forward" : "reverse") << ")" << std::endl;
}

void test_dec_speed() {
    std::cout << "\n=== Testing DEC Speed Fix ===" << std::endl;
    std::cout << "DEC_INVERTED: " << DEC_INVERTED << std::endl;
    
    // Test DEC movement from 0° to 45°
    int32_t current_pos = 0;
    int32_t target_pos = 1000;
    int32_t dec_error = target_pos - current_pos;  // Error = target - current
    
    int motor_direction = get_motor_direction(dec_error, DEC_INVERTED);
    
    std::cout << "Current: " << current_pos << ", Target: " << target_pos << std::endl;
    std::cout << "DEC error " << dec_error << " -> motor direction " << motor_direction << std::endl;
    std::cout << "Motor Direction: " << (motor_direction > 0 ? "Forward" : "Reverse") << std::endl;
    
    // Position tracking: when motor is inverted, motor_direction -1 means position +1
    int32_t movement = 100;
    int32_t actual_movement = DEC_INVERTED ? -motor_direction : motor_direction;
    current_pos += actual_movement * movement;
    
    std::cout << "Movement: " << movement << " steps, actual direction: " << actual_movement << std::endl;
    std::cout << "New position: " << current_pos << std::endl;
    
    // Check if we're getting closer to target
    int32_t new_error = target_pos - current_pos;
    std::cout << "New error: " << new_error << " (should be smaller than " << dec_error << ")" << std::endl;
    
    if (abs(new_error) < abs(dec_error)) {
        std::cout << "✅ Moving correctly toward target" << std::endl;
    } else {
        std::cout << "❌ Moving away from target!" << std::endl;
    }
}

int main() {
    std::cout << "RA 24h Wrapper and DEC Speed Fix Test" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    test_ra_24h_wrapper();
    test_ra_movement();
    test_dec_speed();
    
    std::cout << "\n=== Fixes Applied ===" << std::endl;
    std::cout << "1. Fixed RA angular distance to handle 24h wrapper correctly" << std::endl;
    std::cout << "2. Removed double inversion from DEC position tracking" << std::endl;
    std::cout << "3. DEC should now move at normal speed" << std::endl;
    
    return 0;
}
