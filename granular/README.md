# GRAINFORFEIT: granular texture engine

An audio effect (VST3, plus AU on Mac) for turning any sound into new textures. It keeps recording the last 4 seconds of whatever you feed it. It replays that audio as tiny overlapping grains, which can be pitched, reversed, scattered, sequenced to your tempo and modulated, then sends them through distortion, a filter, echo and reverb.

**Download:** the latest build is on the repository's Releases page under **GRAINFORFEIT (latest build)**.

## Install

| System | File | Folder |
| --- | --- | --- |
| Windows | `GRAINFORFEIT.vst3` | `C:\Program Files\Common Files\VST3` |
| Mac | `GRAINFORFEIT.vst3` | `~/Library/Audio/Plug-Ins/VST3` |
| Mac (Logic, GarageBand) | `GRAINFORFEIT.component` | `~/Library/Audio/Plug-Ins/Components` |

Rescan plugins in your DAW, then put GRAINFORFEIT on an audio track or bus as an effect. The window can be resized from its bottom-right corner.

## Presets

Pick one from the **Preset** menu at the top: Init, Glass Cloud, Stutter Grid, Reverse Bloom, Octave Rain, Broken Radio, Bitcrushed Swarm, Frozen Choir, Sub Smear, Tape Warp and Minor Halo. Frozen Choir is made for **Freeze**: play a chord, press Freeze, and let it evolve.

## Controls

**Grains**
- **Size:** length of each grain (10 ms to 500 ms). Short is buzzy and glitchy, long is smooth and smeared.
- **Density:** how many grains start each second (when Rhythm is Free).
- **Rhythm:** Free, or lock grains to your project tempo (1/4 to 1/32, including triplets) for rhythmic stutters.
- **Position:** how far back in the recording grains start (0 to 2 s).
- **Spray:** random extra distance added to Position, for scattered, cloud-like textures.
- **Scan:** slowly moves the start point through the recording, forwards or backwards. With Freeze on, this stretches the frozen audio in time without changing its pitch.
- **Shape:** each grain's envelope. Smooth is soft and blurry, Flat is fuller, and Pluck is sharp and percussive.
- **Feedback:** sends the grains back into the recording, so the texture builds on itself.

**Pitch**
- **Pitch:** shifts every grain (±24 semitones).
- **Random:** random pitch per grain, up to ±24 semitones.
- **Scale:** snaps the random pitches to Octaves, Fifths, Major, Minor or Penta (pentatonic), so the shimmer stays musical. Free leaves them unsnapped.
- **Reverse:** chance that a grain plays backwards.
- **Spread:** spreads grains across the stereo field.

**Distortion**: **Type** (Soft, Hard, Fold or Crush) and **Drive**. Levels are compensated, so more drive means dirtier rather than louder.

**Filter**: **Mode** (Low, Band or High), **Cutoff** and **Resonance**.

**Space**
- **Delay:** echo time.
- **Repeats:** echo feedback.
- **Echo:** how much echo you hear.
- **Room:** reverb size.
- **Reverb:** how much reverb you hear.

**Output**: **Mix** (original sound versus the grains) and **Gain**.

**LFO 1 and LFO 2** move a control up and down automatically. Choose the **Wave** (Sine, Triangle, Square or Random), **Rate**, **Depth** and **Target**. Targets are Position, Spray, Size, Density, Scan, Shape, Pitch, Random, Spread, Drive, Cutoff, Echo, Reverb and Mix.

**XY pad**: drag the dot to push two controls at once. Choose them with the **X** and **Y** menus under the pad. The centre leaves them unchanged, and a double-click resets the dot to the centre. This works well for performing live, or for automating from your DAW.

When an LFO or the pad is moving a knob, a small ring on that knob's track shows where it's being pushed in real time.

**Freeze** stops recording, so the grains keep playing from the audio already captured.

## The display

- **Top:** the last 4 seconds of recorded audio, newest on the right. The shaded band shows where new grains can start (Position, Scan and Spray). Each dot is a grain playing right now:
  - **Across:** where in the recording it is reading from.
  - **Up or down:** its pitch.
  - **Size:** its loudness.
  - **Hollow:** it is playing backwards.
- **Bottom:** a live spectrum of the output, from 20 Hz to 20 kHz.

## Building

GitHub builds it automatically whenever anything in this folder changes (see the Actions tab). To build it yourself on a computer with CMake and Visual Studio or Xcode, run this from this folder:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
