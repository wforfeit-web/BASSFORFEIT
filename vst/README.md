# BASSFORFEIT — VST3 / AU bass synth

A mono bass synth for EDM and dubstep: two oscillators plus a sub, a resonant filter with drive, a wobble that locks to your DAW's tempo, a vowel-based scream layer, and 7 presets (Classic wobble, Talking growl, Reese, Square stab, Deep sub, Brostep scream, Yoi scream).

Built with JUCE 8.

## Easiest: build on GitHub (no Visual Studio or Xcode needed)

GitHub builds the plugin for you on its own Windows and Mac machines.

1. Sign in at github.com, click **+** (top right) → **New repository**, name it `BASSFORFEIT`, choose **Private**, and click **Create repository**.
2. On the new repository's page, click **uploading an existing file**. Unzip `BASSFORFEIT.zip` on your computer, open the BASSFORFEIT folder, select **everything inside it** (including the hidden `.github` folder), drag it into the page, and click **Commit changes**.
   - If `.github` doesn't upload (hidden folders are sometimes skipped), click **Add file → Create new file**, type `.github/workflows/build.yml` as the name, paste in the contents of that file, and commit.
3. Click the **Actions** tab. A run called **Build BASSFORFEIT** starts by itself (if Actions asks to be enabled, click the green button, then **Run workflow**). It takes about 5–10 minutes.
4. When both jobs show a green tick, click the run and scroll to **Artifacts**:
   - **BASSFORFEIT-Windows-VST3**: unzip it and copy `BASSFORFEIT.vst3` to `C:\Program Files\Common Files\VST3`.
   - **BASSFORFEIT-Mac**: unzip it, then unzip the VST3 and/or AU zip inside, and copy to the Mac folders in step 3 below.
5. Rescan plugins in your DAW.

Every time you change a file in the repository, GitHub rebuilds it automatically.

## Building on your own computer

You need a Windows PC or a Mac.

## Files

```
BASSFORFEIT/
  CMakeLists.txt          build script (GitHub and Option B)
  .github/workflows/      GitHub build instructions
  Source/
    PluginProcessor.h     sound engine: parameters, presets, DSP
    PluginProcessor.cpp
    PluginEditor.h        the window: white and blue, Helvetica, knobs
    PluginEditor.cpp
```

## 1. Install a compiler

- **Windows:** install Visual Studio Community (free). In the installer, tick **Desktop development with C++**.
- **Mac:** install **Xcode** from the App Store, open it once, and accept the licence.

## 2. Get JUCE

Download JUCE from juce.com (or clone github.com/juce-framework/JUCE). Check that JUCE's current licence terms fit how you plan to use or sell the plugin.

## Option A: Projucer (easiest)

1. Open **Projucer** (it's inside the JUCE folder).
2. Choose **New Project → Plug-In → Basic** and name it **BASSFORFEIT**.
3. In the project settings (the gear icon):
   - Plugin Formats: tick **VST3** (and **AU** on a Mac). Tick **Standalone** too if you want an app you can test without a DAW.
   - Plugin Characteristics: tick **Plugin is a Synth** and **Plugin MIDI Input**.
   - Plugin Manufacturer Code: `Bsff`. Plugin Code: `Bsf1`.
   - C++ Language Standard: **C++17** or newer.
4. In the project's **Source** folder on disk, replace the four files (`PluginProcessor.h/.cpp`, `PluginEditor.h/.cpp`) with the ones from this folder. Keep the file names exactly the same.
5. Click the export button at the top to open the project in **Visual Studio** or **Xcode**.
6. Switch the build configuration to **Release** and build:
   - Visual Studio: pick the `BASSFORFEIT_VST3` target, then **Build → Build Solution**.
   - Xcode: pick the `BASSFORFEIT - VST3` (or AU) scheme, then **Product → Build**.

## Option B: CMake

1. In a terminal, from the BASSFORFEIT folder (JUCE 8.0.9 is downloaded automatically; to use your own copy, put it in this folder named `JUCE`):

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The built plugins end up under `build/BASSFORFEIT_artefacts/Release/`.

## 3. Install the plugin

Copy the built plugin to your system's plugin folder:

| System | Format | Folder |
| --- | --- | --- |
| Windows | `BASSFORFEIT.vst3` | `C:\Program Files\Common Files\VST3` |
| Mac | `BASSFORFEIT.vst3` | `~/Library/Audio/Plug-Ins/VST3` |
| Mac | `BASSFORFEIT.component` (AU, for Logic/GarageBand) | `~/Library/Audio/Plug-Ins/Components` |

Then rescan plugins in your DAW and load BASSFORFEIT on an instrument/MIDI track.

## Using it

- Play it from a MIDI keyboard or the piano roll. It is mono: overlapping notes slide into each other using **Glide**.
- The wobble locks to your project tempo and bar position while the DAW is playing. Rates run from 1/1 to 1/32, including triplets.
- Presets are in the **Preset** menu at the top right, and also show up as programs in your DAW.
- Double-click any knob to reset it.
- **Scream:** Scream blends in the layer, Vowel moves it from OO to EE, Talk ties the vowel to the wobble (the "yoi" talking effect), and Bite makes it harsher.

## Fonts

The Mac build uses Helvetica Neue. Windows doesn't ship with Helvetica, so the Windows build uses Arial, which has the same letter shapes and spacing. If you have Helvetica installed on Windows, change `"Arial"` to `"Helvetica"` at the top of `PluginEditor.cpp`.

## Troubleshooting

- **The plugin doesn't appear in the DAW:** check it's the VST3 (not the Standalone) you copied, that it's in the folder above, and rescan. FL Studio: Options → Manage plugins → Find plugins.
- **Mac says the plugin is damaged or can't be opened:** it's unsigned. In Terminal run `xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/BASSFORFEIT.vst3`.
- **Build errors mentioning `getPosition` or `ParameterID`:** update to JUCE 7 or newer.
- **A GitHub run shows a red X:** click the failed job, copy the red error lines, and send them to Claude.
