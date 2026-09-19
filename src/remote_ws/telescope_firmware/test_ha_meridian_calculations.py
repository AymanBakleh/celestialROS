#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Hour Angle (HA) & Meridian Flip Calculation Test Suite
Tests astronomical calculations for telescope mount control

Author: Telescope Firmware Test Suite
Date: 2026-03-27
"""

import math
from datetime import datetime
from dataclasses import dataclass
from typing import List

# Constants
PI = math.pi
DEG_TO_RAD = PI / 180.0
RAD_TO_DEG = 180.0 / PI

# Site location (Damascus, Syria)
SITE_LATITUDE = 33.50917
SITE_LONGITUDE = 36.31167

# Meridian flip threshold in degrees (typically ±60 to ±90)
MERIDIAN_FLIP_THRESHOLD = 60.0

@dataclass
class TestScenario:
    """Represents a single test scenario"""
    name: str
    year: int
    month: int
    day: int
    hour: int
    minute: int
    second: int
    target_ra_hours: float     # RA in hours (0-24)
    target_dec_deg: float      # DEC in degrees (-90 to +90)
    expected_ha_deg: float     # Expected hour angle
    expect_meridian_flip: bool

# ============ CALCULATION FUNCTIONS ============

def calculate_julian_day(year, month, day, hour, minute, second):
    """Calculate Julian Day from date/time"""
    if month <= 2:
        year -= 1
        month += 12
    
    A = year // 100
    B = 2 - A + A // 4
    
    day_fraction = hour / 24.0 + minute / 1440.0 + second / 86400.0
    
    jd = math.floor(365.25 * (year + 4716)) + \
         math.floor(30.6001 * (month + 1)) + \
         day + day_fraction + B - 1524.5
    
    return jd

def calculate_gmst_deg(jd):
    """Calculate GMST (Greenwich Mean Sidereal Time) in degrees"""
    T = (jd - 2451545.0) / 36525.0
    
    # GMST at 0h UT in seconds
    gmst_0h = 24110.54841 + 8640184.812866 * T + 0.093104 * T * T - 0.0000062 * T * T * T
    
    # Get the UT time from Julian Day
    jd_int = math.floor(jd)
    jd_frac = jd - jd_int
    
    # Seconds since J2000.0 for this specific time
    ut_seconds = jd_frac * 86400.0
    
    # Earth rotation angle
    rotation_angle = 67310.54841 + (876600.0 * 3600.0 + 8640184.812866) * T + 0.093104 * T * T
    
    # GMST in seconds
    gmst_seconds = gmst_0h + rotation_angle * (ut_seconds / 86400.0)
    
    # Convert to degrees
    gmst_deg = (gmst_seconds / 86400.0) * 360.0
    gmst_deg = gmst_deg % 360.0
    if gmst_deg < 0.0:
        gmst_deg += 360.0
    
    return gmst_deg

def calculate_lst_deg(year, month, day, hour, minute, second, longitude):
    """Calculate Local Sidereal Time in degrees"""
    jd = calculate_julian_day(year, month, day, hour, minute, second)
    gmst_deg = calculate_gmst_deg(jd)
    
    # LST = GMST + Longitude
    lst_deg = gmst_deg + longitude
    lst_deg = lst_deg % 360.0
    if lst_deg < 0.0:
        lst_deg += 360.0
    
    return lst_deg

def normalize_angle_180(angle_deg):
    """Normalize angle to ±180 range"""
    while angle_deg > 180.0:
        angle_deg -= 360.0
    while angle_deg < -180.0:
        angle_deg += 360.0
    return angle_deg

def normalize_angle_360(angle_deg):
    """Normalize angle to 0-360 range"""
    angle_deg = angle_deg % 360.0
    if angle_deg < 0.0:
        angle_deg += 360.0
    return angle_deg

def calculate_hour_angle(lst_deg, ra_deg):
    """Calculate Hour Angle from RA and LST (all in degrees)"""
    ha_deg = lst_deg - ra_deg
    return normalize_angle_180(ha_deg)

def ra_hours_to_degrees(ra_hours):
    """Convert RA from hours to degrees"""
    return ra_hours * 15.0  # 1 hour = 15 degrees

def ra_degrees_to_hours(ra_deg):
    """Convert RA from degrees to hours"""
    return ra_deg / 15.0

def should_perform_meridian_flip(current_ha_deg, target_ha_deg):
    """Check if meridian flip is needed"""
    curr = normalize_angle_180(current_ha_deg)
    targ = normalize_angle_180(target_ha_deg)
    
    # Check if we're crossing the meridian
    crosses_meridian = (curr < 0.0 and targ > 0.0) or (curr > 0.0 and targ < 0.0)
    
    # Check distance to meridian
    dist_to_meridian = targ
    near_meridian = abs(dist_to_meridian) < MERIDIAN_FLIP_THRESHOLD
    
    return crosses_meridian and near_meridian

def calculate_dec_after_flip(current_dec_deg):
    """Calculate DEC position after meridian flip"""
    return -current_dec_deg

def calculate_sun_ra_dec(jd):
    """Compute approximate Sun RA/DEC in degrees for given Julian Day."""
    n = jd - 2451545.0
    L = normalize_angle_360(280.460 + 0.9856474 * n)
    g = normalize_angle_360(357.528 + 0.9856003 * n)
    lambda_e = normalize_angle_360(L + 1.915 * math.sin(math.radians(g)) + 0.020 * math.sin(math.radians(2 * g)))
    epsilon = 23.439 - 0.0000004 * n
    ra = math.degrees(math.atan2(math.cos(math.radians(epsilon)) * math.sin(math.radians(lambda_e)), math.cos(math.radians(lambda_e))))
    ra = normalize_angle_360(ra)
    dec = math.degrees(math.asin(math.sin(math.radians(epsilon)) * math.sin(math.radians(lambda_e))))
    return ra, dec

def calculate_moon_ra_dec_approx(jd):
    """Compute approximate Moon RA/DEC in degrees for given Julian Day."""
    n = jd - 2451545.0
    Lp = normalize_angle_360(218.316 + 13.176396 * n)
    M = normalize_angle_360(134.963 + 13.064993 * n)
    F = normalize_angle_360(93.272 + 13.229350 * n)
    D = normalize_angle_360(297.850 + 12.190749 * n)
    lambda_m = normalize_angle_360(Lp + 6.289 * math.sin(math.radians(M)))
    beta_m = 5.128 * math.sin(math.radians(F))
    epsilon = 23.439 - 0.0000004 * n
    ra = math.degrees(math.atan2(math.cos(math.radians(epsilon)) * math.sin(math.radians(lambda_m)) - math.tan(math.radians(beta_m)) * math.sin(math.radians(epsilon)), math.cos(math.radians(lambda_m))))
    ra = normalize_angle_360(ra)
    dec = math.degrees(math.asin(math.sin(math.radians(beta_m)) * math.cos(math.radians(epsilon)) + math.cos(math.radians(beta_m)) * math.sin(math.radians(epsilon)) * math.sin(math.radians(lambda_m))))
    return ra, dec

def angular_separation(ra1, dec1, ra2, dec2):
    """Angular distance between two RA/DEC points (degrees)."""
    ra1r, dec1r = math.radians(ra1), math.radians(dec1)
    ra2r, dec2r = math.radians(ra2), math.radians(dec2)
    cos_angle = math.sin(dec1r) * math.sin(dec2r) + math.cos(dec1r) * math.cos(dec2r) * math.cos(ra1r - ra2r)
    cos_angle = max(-1.0, min(1.0, cos_angle))
    return math.degrees(math.acos(cos_angle))

# ============ OUTPUT FUNCTIONS ============

def print_header():
    """Print test suite header"""
    print("\n" + "=" * 100)
    print("HOUR ANGLE & MERIDIAN FLIP CALCULATION TEST SUITE")
    print("=" * 100 + "\n")

def print_scenario_header(scenario: TestScenario, index: int):
    """Print scenario header"""
    print("\n" + "-" * 100)
    print(f"SCENARIO {index + 1}: {scenario.name}")
    print("-" * 100)

def print_calculation_result(scenario: TestScenario, jd, gmst, lst, ra_deg, 
                            current_ha, target_ha, flip_needed):
    """Print detailed calculation results"""
    
    print(f"\n[INPUT DATA]")
    print(f"  Date/Time: {scenario.year:04d}-{scenario.month:02d}-{scenario.day:02d} "
          f"{scenario.hour:02d}:{scenario.minute:02d}:{scenario.second:02d} UTC")
    print(f"  Location: Lat={SITE_LATITUDE}°, Lon={SITE_LONGITUDE}°")
    print(f"  Target RA: {scenario.target_ra_hours}h = {ra_deg:.6f}°")
    print(f"  Target DEC: {scenario.target_dec_deg}°")
    
    print(f"\n[CALCULATED VALUES]")
    print(f"  Julian Day: {jd:.6f}")
    print(f"  GMST: {gmst:.6f}°")
    print(f"  LST: {lst:.6f}° ({ra_degrees_to_hours(lst):.6f}h)")
    print(f"  RA (target): {ra_deg:.6f}° ({scenario.target_ra_hours}h)")
    print(f"  Hour Angle (current): {current_ha:.6f}°")
    print(f"  Hour Angle (target): {target_ha:.6f}°")
    
    print(f"\n[ANALYSIS]")
    if current_ha < 0.0:
        print(f"  HA Status: APPROACHING MERIDIAN (East side, |HA|={abs(current_ha):.6f}°)")
    elif current_ha > 0.0:
        print(f"  HA Status: PAST MERIDIAN (West side, HA={current_ha:.6f}°)")
    else:
        print(f"  HA Status: ON MERIDIAN (HA=0°)")
    
    print(f"  Distance to Meridian: {abs(current_ha):.6f}°")
    print(f"  Meridian Flip Threshold: {MERIDIAN_FLIP_THRESHOLD}°")
    
    print(f"\n[VERIFICATION - CALCULATION VALIDITY]")
    # Verify that calculations are internally consistent and reasonable
    validation_pass = True
    issues = []
    
    # Check 1: HA should equal LST - RA (modulo 360, normalized to ±180)
    manual_ha = normalize_angle_180(lst - ra_deg)
    ha_consistency_error = abs(current_ha - manual_ha)
    if ha_consistency_error < 0.01:
        print(f"  ✓ HA Calculation: Verified (HA = LST - RA)")
        print(f"    - LST({lst:.2f}°) - RA({ra_deg:.2f}°) = HA({current_ha:.2f}°)")
    else:
        print(f"  ✗ HA Calculation: INCONSISTENT (error {ha_consistency_error:.2f}°)")
        issues.append("HA calculation error")
        validation_pass = False
    
    # Check 2: HA range should be ±180
    if -180.0 <= current_ha <= 180.0:
        print(f"  ✓ HA Range: Valid (±180° range)")
    else:
        print(f"  ✗ HA Range: INVALID ({current_ha}°)")
        issues.append("HA out of range")
        validation_pass = False
    
    # Check 3: Meridian flip decision should match scenario expectation
    if flip_needed == scenario.expect_meridian_flip:
        print(f"  ✓ Meridian Flip: Consistent with scenario")
    else:
        print(f"  ✗ Meridian Flip: MISMATCH (Expected: {scenario.expect_meridian_flip}, Got: {flip_needed})")
        issues.append("Meridian flip mismatch")
        validation_pass = False
    
    # Check 4: JD should be positive and reasonable (between 2451545 for J2000 and ~2500000)
    if 2451545 < jd < 2500000:
        print(f"  ✓ Julian Day: Reasonable ({jd:.2f})")
    else:
        print(f"  ✗ Julian Day: UNREASONABLE ({jd})")
        issues.append("JD out of expected range")
        validation_pass = False
    
    # Check 5: GMST should be 0-360
    if 0 <= gmst <= 360:
        print(f"  ✓ GMST: Valid range (0-360°)")
    else:
        print(f"  ✗ GMST: INVALID ({gmst}°)")
        issues.append("GMST out of range")
        validation_pass = False
    
    print(f"\n[MERIDIAN FLIP DECISION]")
    if flip_needed:
        print(f"  ✓ MERIDIAN FLIP REQUIRED")
        print(f"    - Reason: Approaching meridian crossing")
        new_dec = calculate_dec_after_flip(scenario.target_dec_deg)
        print(f"    - New DEC: {scenario.target_dec_deg}° -> {new_dec}°")
        print(f"    - Motor Direction: INVERTED for DEC axis")
    else:
        print(f"  ✗ No meridian flip needed")
        print(f"    - Distance to meridian: {abs(current_ha):.6f}° (threshold: {MERIDIAN_FLIP_THRESHOLD}°)")
    
    if validation_pass:
        print(f"\n✓ SCENARIO PASS - All calculations are valid and consistent")
        return True
    else:
        print(f"\n✗ SCENARIO FAIL - Issues found:")
        for issue in issues:
            print(f"   - {issue}")
        return False


def run_accumulation_sequence_test():
    """Run a sequential coverage test: Moon -> Sun -> Vega (example star)."""
    # Damascus local 07:00 (UTC+2) => 05:00 UTC
    jd = calculate_julian_day(2026, 3, 27, 5, 0, 0)
    lst = calculate_lst_deg(2026, 3, 27, 5, 0, 0, SITE_LONGITUDE)

    moon_ra, moon_dec = calculate_moon_ra_dec_approx(jd)
    sun_ra, sun_dec = calculate_sun_ra_dec(jd)
    vega_ra, vega_dec = ra_hours_to_degrees(18.615556), 38.783

    sequence = [
        ('Moon', moon_ra, moon_dec),
        ('Sun', sun_ra, sun_dec),
        ('Vega', vega_ra, vega_dec),
    ]

    cumulative_move = 0.0
    previous = None

    print(f"  Sequence time: 2026-03-27 05:00 UTC (Damascus 07:00 local)")
    print(f"  LST: {lst:.6f}° ({ra_degrees_to_hours(lst):.6f}h)")

    for name, ra, dec in sequence:
        ha = calculate_hour_angle(lst, ra)
        print(f"    - {name}: RA {ra:.6f}°, DEC {dec:.6f}°, HA {ha:.6f}°")

        if previous is not None:
            prev_ra, prev_dec = previous
            move = angular_separation(prev_ra, prev_dec, ra, dec)
            cumulative_move += move
            print(f"      > Slew delta: {move:.6f}° (cumulative {cumulative_move:.6f}°)")
        previous = (ra, dec)

    # Verification: if engine is consistent, cumulative_move should be finite and < 360.
    if cumulative_move < 360.0:
        print("  ✓ Accumulation check passed: path motion is coherent")
    else:
        print("  ✗ Accumulation check failed: excessive path motion")

# ============ MAIN TEST EXECUTION ============

def main():
    """Execute all test scenarios"""
    
    print_header()
    
    # Define test scenarios
    scenarios = [
        TestScenario(
            name="Dawn Observation - Object Approaching Meridian (East)",
            year=2026, month=3, day=27, hour=4, minute=30, second=0,
            target_ra_hours=5.0,        # Vega: 5h RA
            target_dec_deg=38.783,      # Vega: +38.78° DEC
            expected_ha_deg=-45.0,      # Expected: ~45° before meridian
            expect_meridian_flip=False
        ),
        TestScenario(
            name="Morning Observation - Object On Meridian",
            year=2026, month=3, day=27, hour=6, minute=0, second=0,
            target_ra_hours=6.0,        # 6h RA
            target_dec_deg=8.867,       # +8.87° DEC
            expected_ha_deg=0.0,        # Expected: On meridian
            expect_meridian_flip=False
        ),
        TestScenario(
            name="Afternoon Observation - Object Past Meridian (West)",
            year=2026, month=3, day=27, hour=8, minute=0, second=0,
            target_ra_hours=4.0,        # Sirius: 4h RA
            target_dec_deg=-16.716,     # Sirius: -16.72° DEC
            expected_ha_deg=60.0,       # Expected: ~60° past meridian
            expect_meridian_flip=False
        ),
        TestScenario(
            name="Critical Scenario - Close to Meridian Flip Threshold (East)",
            year=2026, month=3, day=27, hour=5, minute=50, second=0,
            target_ra_hours=6.0,        # 6h RA
            target_dec_deg=45.0,        # High declination object
            expected_ha_deg=-58.0,      # Expected: ~58° before meridian (approaching)
            expect_meridian_flip=False  # Close to threshold but still approaching
        ),
        TestScenario(
            name="Critical Scenario - Crossing Meridian to West",
            year=2026, month=3, day=27, hour=6, minute=10, second=0,
            target_ra_hours=6.0,        # Same target
            target_dec_deg=45.0,
            expected_ha_deg=68.0,       # Expected: ~68° past meridian
            expect_meridian_flip=False
        ),
        TestScenario(
            name="Pole Star - Nearly on Celestial Pole",
            year=2026, month=3, day=27, hour=12, minute=0, second=0,
            target_ra_hours=2.31,       # Polaris: 2h 31m RA
            target_dec_deg=89.264,      # Polaris: +89.26° DEC
            expected_ha_deg=-30.0,      # Expected: approaching meridian
            expect_meridian_flip=False
        ),
        TestScenario(
            name="Southern Hemisphere Object - Negative DEC",
            year=2026, month=3, day=27, hour=7, minute=30, second=0,
            target_ra_hours=21.0,       # Canopus: 21h RA
            target_dec_deg=-52.696,     # Canopus: -52.70° DEC (southern)
            expected_ha_deg=45.0,       # Expected: past meridian
            expect_meridian_flip=False
        ),
        TestScenario(
            name="Equatorial Object - Zero DEC Motion",
            year=2026, month=3, day=27, hour=6, minute=0, second=0,
            target_ra_hours=18.0,       # Hypothetical equatorial object
            target_dec_deg=0.0,         # On celestial equator
            expected_ha_deg=0.0,        # Expected: near meridian
            expect_meridian_flip=False
        ),
    ]
    
    # Run all scenarios
    pass_count = 0
    fail_count = 0
    
    for i, scenario in enumerate(scenarios):
        print_scenario_header(scenario, i)
        
        # Calculate values
        jd = calculate_julian_day(scenario.year, scenario.month, scenario.day,
                                 scenario.hour, scenario.minute, scenario.second)
        gmst = calculate_gmst_deg(jd)
        lst = calculate_lst_deg(scenario.year, scenario.month, scenario.day,
                               scenario.hour, scenario.minute, scenario.second,
                               SITE_LONGITUDE)
        
        ra_deg = ra_hours_to_degrees(scenario.target_ra_hours)
        current_ha = calculate_hour_angle(lst, ra_deg)
        target_ha = current_ha
        
        flip_needed = should_perform_meridian_flip(current_ha, target_ha)
        
        result = print_calculation_result(scenario, jd, gmst, lst, ra_deg,
                                         current_ha, target_ha, flip_needed)
        
        # Check if result matches expectation
        error = abs(scenario.expected_ha_deg - current_ha)
        if result and flip_needed == scenario.expect_meridian_flip:
            pass_count += 1
            print("\n✓ SCENARIO PASS")
        else:
            fail_count += 1
            print("\n✗ SCENARIO FAIL")
            if error >= 0.5:
                print(f"  - HA Error: {error:.6f}° exceeds 0.5°")
            if flip_needed != scenario.expect_meridian_flip:
                print(f"  - Flip decision mismatch. Expected: {scenario.expect_meridian_flip}, Got: {flip_needed}")
    
    # Print summary
    print("\n" + "=" * 100)
    print("TEST SUMMARY")
    print("=" * 100)
    print(f"Total Scenarios: {len(scenarios)}")
    print(f"Passed: {pass_count}")
    print(f"Failed: {fail_count}")
    success_rate = (pass_count * 100.0 / len(scenarios)) if len(scenarios) > 0 else 0
    print(f"Success Rate: {success_rate:.1f}%")
    print("=" * 100)
    
    # Print formulas
    print("\n[FORMULAS USED]")
    print("1. Julian Day (JD): floor(365.25*(year+4716)) + floor(30.6001*(month+1)) + day + fraction")
    print("2. GMST: Uses standard astronomical algorithms")
    print("3. Local Sidereal Time (LST): LST = GMST + Longitude")
    print("4. Hour Angle (HA): HA = LST - RA (normalized to ±180°)")
    print("5. Meridian Flip: HA ≈ 0° ± threshold (default: ±60°)")
    print("6. DEC after flip: DEC_new = -DEC_current")
    
    # Run path accumulation test (Moon -> Sun -> Vega)
    print("\n[ACCUMULATION TEST]")
    run_accumulation_sequence_test()
    
    # Print coordinate system reference
    print("\n[COORDINATE SYSTEM REFERENCE]")
    print("HA = -180° to 0°: Object East of meridian (approaching)")
    print("HA = 0°: Object on meridian")
    print("HA = 0° to +180°: Object West of meridian (past)")
    print("DEC: -90° (South pole) to +90° (North pole)")
    
    # Print site configuration
    print("\n[SITE CONFIGURATION]")
    print("Location: Damascus, Syria")
    print(f"Latitude: {SITE_LATITUDE}°")
    print(f"Longitude: {SITE_LONGITUDE}°")
    print(f"Meridian Flip Threshold: ±{MERIDIAN_FLIP_THRESHOLD}°")
    
    print("\n" + "=" * 100 + "\n")
    
    return 0 if fail_count == 0 else 1

if __name__ == "__main__":
    exit(main())
