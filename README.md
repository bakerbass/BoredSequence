# BoredSequence

A minimal 8‑step sequencer for Daisy Field. Each of the 8 knobs sets one step’s CV level; the sequence is output on CV Out 1. Knob LEDs indicate the active step.

## Build and program

Use the provided VS Code tasks:
- build — compiles the project
- build_and_program — compiles and programs via SWD
- build_and_program_dfu — compiles and programs via DFU

## How it works
- Knobs 1–8 set per‑step voltage (0–5V range).
- Sequence advances at a fixed 120 BPM.
- Current step LED is brightest during gate.

## Requirements
- libDaisy and DaisySP checked out at `../DaisyExamples/`
- ARM GCC toolchain in PATH

