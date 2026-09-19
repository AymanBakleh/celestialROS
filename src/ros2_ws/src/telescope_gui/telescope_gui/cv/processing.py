from __future__ import annotations

import numpy as np


def to_float_frame(frame: np.ndarray) -> np.ndarray:
    array = np.asarray(frame, dtype=np.float32)
    if array.max(initial=0) > 1.0:
        array /= 255.0
    return np.clip(array, 0.0, 1.0)


def stretch_image(frame: np.ndarray, low_percentile: float = 1.0, high_percentile: float = 99.5) -> np.ndarray:
    array = to_float_frame(frame)
    low = np.percentile(array, low_percentile)
    high = np.percentile(array, high_percentile)
    if high <= low:
        return (array * 255).astype(np.uint8)
    stretched = np.clip((array - low) / (high - low), 0.0, 1.0)
    stretched = np.power(stretched, 0.85)
    return (stretched * 255).astype(np.uint8)


def histogram_rgb(frame: np.ndarray) -> dict[str, list[int]]:
    array = np.asarray(frame)
    if array.ndim == 2:
        array = np.repeat(array[..., None], 3, axis=2)
    channels = {"red": array[..., 0], "green": array[..., 1], "blue": array[..., 2]}
    histograms: dict[str, list[int]] = {}
    for name, channel in channels.items():
        hist, _ = np.histogram(channel, bins=256, range=(0, 255))
        histograms[name] = hist.astype(int).tolist()
    return histograms


def focus_score(frame: np.ndarray) -> float:
    array = to_float_frame(frame)
    gray = array.mean(axis=2) if array.ndim == 3 else array
    laplacian = (
        -4.0 * gray
        + np.roll(gray, 1, axis=0)
        + np.roll(gray, -1, axis=0)
        + np.roll(gray, 1, axis=1)
        + np.roll(gray, -1, axis=1)
    )
    return float(np.var(laplacian))


def frame_summary(frame: np.ndarray) -> dict[str, float | int]:
    array = np.asarray(frame)
    luminance = array.mean(axis=2) if array.ndim == 3 else array
    return {
        "mean": float(np.mean(luminance)),
        "median": float(np.median(luminance)),
        "max": int(np.max(luminance)),
        "min": int(np.min(luminance)),
        "focus_score": focus_score(array),
    }
