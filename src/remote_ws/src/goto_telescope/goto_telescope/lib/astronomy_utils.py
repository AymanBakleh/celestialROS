"""Astronomy helper functions (minimal implementations)."""
import math


def angular_distance(ra1_h, dec1_deg, ra2_h, dec2_deg):
    """Compute approximate angular distance in degrees between two RA/Dec points.

    RA inputs are in hours; Dec in degrees. Result is degrees.
    """
    # convert RA hours to degrees
    ra1 = ra1_h * 15.0
    ra2 = ra2_h * 15.0

    # convert to radians
    ra1_r = math.radians(ra1)
    ra2_r = math.radians(ra2)
    dec1_r = math.radians(dec1_deg)
    dec2_r = math.radians(dec2_deg)

    # spherical law of cosines
    cos_ang = (math.sin(dec1_r) * math.sin(dec2_r) +
               math.cos(dec1_r) * math.cos(dec2_r) * math.cos(ra1_r - ra2_r))
    cos_ang = max(-1.0, min(1.0, cos_ang))
    ang_r = math.acos(cos_ang)
    return math.degrees(ang_r)
