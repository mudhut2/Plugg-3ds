# Plugg3DS

A plugg-inspired drum machine for the Nintendo 3DS. Designed as a lightweight, experimental platform for audio playback and future sequencing.

## Features
- Real-time drum pad triggering via touchscreen and buttons  
- Classic plugg drum samples (808s, snares, hi-hats)  
- Low-latency audio playback on 3DS hardware
- Browse/load custom sounds onto pads   
- Modular codebase for experimentation and extension  

## Installation
1. Copy `Plugg3DS.3dsx` to your SD card under `/3ds/Plugg3DS/`  
2. Boot via Homebrew Launcher
3. Put custom sounds in "sdmc:/sounds"

## Controls
- Touchscreen: trigger individual pads  
- D-pad / Buttons: alternative pad triggers
- SELECT: switch between Play and Menu mode

## Technical Details
- Language: C  
- Libraries: libctru, citro2d / citro3d  
- Architecture: main loop handles input, audio mixing, and rendering. File browser module for loading custom sounds  
- Audio system: sample-based playback with fixed buffer sizes, optimized for low latency
- Wav file parser. PCM16 format only due to 3ds contraints. Up to 32 khz.

## Planned Features
- Replace console UI with custom interface
- Step sequencer    
- Basic DSP effects (reverb, delay, filter)  
- Save/load patterns  

## License
MIT License — contributions and experimentation welcome  

## Acknowledgments
- devkitPro / 3DS homebrew community  
- BeatPluggz 
