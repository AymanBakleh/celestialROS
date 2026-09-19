# Jarspace Telescope Session Failures and Lessons Report

Date: 2026-04-05
Scope: Attempts that did not solve the Stellarium mismatch or introduced regressions during this session.

## 1. Purpose of This File

This report records what we tried, why we tried it, and why it did not fully work.
It is intended to prevent repeating the same dead ends.

## 2. Attempt Log (Unsuccessful or Regressive)

### Attempt F1: Add extra state source subscription to serial receiver

Why it was tried:
- To improve robustness by pulling current pointing from an additional state stream when merged joints were delayed or missing.

What changed:
- Serial receiver consumed more than one state source (including /telescope/state path during one phase).

What happened:
- Competing updates produced oscillation/shake in simulation and behavior instability.

Final decision:
- Reverted to a single authoritative state source in receiver path.

Lesson:
- For this architecture, multiple live state authorities are worse than brief data gaps.

---

### Attempt F2: Partial convention fix (single component only)

Why it was tried:
- Quick correction in one component to address visible DEC mismatch.

What changed:
- One publisher/consumer pair was corrected first, before full-stack convention migration.

What happened:
- Improvement was local, but mismatch could reappear because other nodes still used previous mapping.

Final decision:
- Perform full convention sweep across all conversion boundaries.

Lesson:
- Coordinate convention fixes must be atomic at system level, not piecemeal.

---

### Attempt F3: Keep mixed old/new DEC formulas during transition

Why it was tried:
- To minimize immediate breakage while transitioning.

What changed:
- Some paths used +90/-90 offset logic while others were already direct-radians.

What happened:
- Produced hidden double-shifts or missing shifts depending on runtime node order.
- Manifested as apparent Stellarium offset returning "for some reason".

Final decision:
- Remove all +90/-90 transforms consistently per your request.

Lesson:
- Mixed conventions create non-deterministic apparent errors.

---

### Attempt F4: Assume pointing error is mainly firmware astronomical math

Why it was tried:
- Firmware contains extensive time/LST/HA logic and was a natural suspect.

What happened:
- The mismatch was also present in fake mode and tied strongly to transport/convention paths.
- Firmware-only focus did not explain all observed symptoms.

Final decision:
- Treat issue as a cross-layer integration problem (bridge + merge + transform + driver boundaries).

Lesson:
- If issue reproduces in fake and real mount, suspect shared pipeline first.

## 3. Current Known Risk After No-Offset Migration

You reported the offset is back after no-offset conversion.
Most probable causes now are:
- A running node from old build/install still applies old transform.
- One remaining runtime path still emits/consumes old DEC joint convention.
- RA unit coercion mismatch at a bridge edge (hours/degrees) creates apparent DEC/RA pointing error.

This is not a conceptual failure of no-offset itself; it is usually a deployment consistency problem or one missed boundary.

## 4. Guardrails for Next Iteration

1. Enforce one state authority for receiver.
2. Enforce one DEC convention everywhere (already targeted to direct radians).
3. Confirm RA unit contracts at each topic and packet boundary.
4. Restart all nodes after rebuild to avoid stale artifacts.
5. Validate each conversion with live topic probes before Stellarium checks.

## 5. Minimal Root-Cause Triage Order

1. Validate /telescope/hardware_joint_states dec_joint interpretation.
2. Validate /joint_states_merged dec_joint equals same convention.
3. Validate /telescope/state dec_deg equals degrees(dec_joint).
4. Validate Stellarium bridge receives/outputs RA units as expected.

## 6. Canonical Status

This file is the canonical failed-attempts and lessons document for this session.
Use this as the "do not repeat" reference.
