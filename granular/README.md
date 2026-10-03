# GRAINFORFEIT: granular sample distortion

An audio effect (VST3, plus AU on Mac). It records whatever you feed it into a 4-second buffer, plays it back as tiny overlapping grains, and distorts the result.

**Download:** the latest build is on the repository's Releases page under **GRAINFORFEIT (latest build)**.

## Install

| System | File | Folder |
| --- | --- | --- |
| Windows | `GRAINFORFEIT.vst3` | `C:\Program Files\Common Files\VST3` |
| Mac | `GRAINFORFEIT.vst3` | `~/Library/Audio/Plug-Ins/VST3` |
| Mac (Logic, GarageBand) | `GRAINFORFEIT.component` | `~/Library/Audio/Plug-Ins/Components` |

Rescan plugins in your DAW, then put GRAINFORFEIT on an audio track or bus as an effect.

## Controls

**Grains**
- **Size:** how long each grain is (10 ms to 500 ms). Short is buzzy and glitchy, long is smooth and smeared.
- **Density:** how many grains start each second.
- **Position:** how far back in the recording grains start (0 to 2 s).
- **Spray:** random extra distance added to Position, for scattered, cloud-like textures.
- **Feedback:** sends the grains back into the recording, so the effect builds on itself.

**Pitch**
- **Pitch:** shifts every grain up or down, in semitones (±24).
- **Random:** random pitch per grain, up to ±12 semitones.
- **Reverse:** chance that a grain plays backwards.
- **Spread:** spreads grains across the stereo field.

**Distortion**
- **Type:** Soft (warm saturation), Hard (clipping), Fold (wavefolding), Crush (bit and sample-rate reduction).
- **Drive:** how hard the grains are pushed into the distortion. Levels are compensated, so it gets dirtier rather than just louder.
- **Tone:** a low-pass filter on the distorted grains. Fully right is open.

**Output**
- **Mix:** balance between your original sound and the grains.
- **Gain:** final output level.

**Freeze** stops recording, so the grains keep playing from the audio already captured. It's great for turning a single hit into a sustained texture.

## The display

- **Top:** the last 4 seconds of recorded audio, newest on the right. The shaded band shows where new grains can start (Position plus Spray), and each dot is a grain that's playing right now. A bigger dot means the grain is louder.
- **Bottom:** a live spectrum of the output, from 20 Hz to 20 kHz.

## Building

GitHub builds it automatically whenever anything in this folder changes (see the Actions tab). To build it yourself on a computer with CMake and Visual Studio or Xcode, run this from this folder:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
