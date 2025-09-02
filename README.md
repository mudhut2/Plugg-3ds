# Plugg3DS

A plugg-inspired drum machine for the Nintendo 3DS. Turns the 3DS into an instrument and utilizes different audio channels for each of the 8 pads. Play preset or custom sounds that you add to your sd card (instructions below).

## Features
- Real-time drum pad triggering via touchscreen and buttons  
- Classic plugg drum samples (808s, snares, hi-hats)  
- Low-latency audio playback on 3DS hardware
- Browse/load custom sounds onto pads   
- Modular codebase for experimentation and extension
  
![IMG_6148](https://github.com/user-attachments/assets/32999d81-a610-4ce0-8f5c-e25a7aa5f6fd)

![IMG_6144](https://github.com/user-attachments/assets/3218afd2-69f0-4abc-bbdb-6a5c4edeb944)

![IMG_6145](https://github.com/user-attachments/assets/4f580373-0dd9-461b-a726-b469df26ace4)

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
