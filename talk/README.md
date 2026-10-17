# TALKFORFEIT: retro speech synth

An instrument plugin (VST3, plus AU on Mac) that turns typed text into a robotic, 90s-computer voice in the spirit of Microsoft Talk It! and its open-source remake OpenTalkIt. You can make it talk, or play it from MIDI so it speaks or sings your words in tune.

The voice comes from TALKFORFEIT's own formant speech engine, written from scratch. It contains no code or sounds from Talk It!, SoftVoice or OpenTalkIt.

**Download:** the latest build is on the repository's Releases page under **TALKFORFEIT (latest build)**.

## Install

| System | File | Folder |
| --- | --- | --- |
| Windows | `TALKFORFEIT.vst3` | `C:\Program Files\Common Files\VST3` |
| Mac | `TALKFORFEIT.vst3` | `~/Library/Audio/Plug-Ins/VST3` |
| Mac (Logic, GarageBand) | `TALKFORFEIT.component` | `~/Library/Audio/Plug-Ins/Components` |

Rescan plugins in your DAW, then load TALKFORFEIT as an instrument on a MIDI track. The window can be resized from its bottom-right corner.

## Making it talk

1. Type into the text box.
2. Press **Talk** to hear it at the Pitch setting. Press it again to stop.
3. Press **Export WAV** to save the whole text as an audio file you can drag into any project.

**Tips**
- Punctuation shapes the delivery. A comma makes a short pause, a full stop makes a longer pause with a falling pitch, and a question mark makes the pitch rise.
- Numbers are read out, so "3.14" is said as "three point one four".
- Made-up words are sounded out letter by letter.
- For exact control, type phonemes in square brackets, like `[HH AH0 L OW1]` for "hello". The digit after each vowel is its stress: 1 is main stress, 2 is secondary, and 0 is unstressed. The phonemes are the standard set used by the CMU Pronouncing Dictionary: AA AE AH AO AW AY B CH D DH EH ER EY F G HH IH IY JH K L M N NG OW OY P R S SH T TH UH UW V W Y Z ZH.

## Playing it from MIDI

**Each note plays** sets what a MIDI note does:
- **Phrase:** each note speaks the whole text at that note's pitch. Playing new notes while still holding one moves the pitch without restarting, so the voice sings the phrase as a melody.
- **Word:** each note speaks the next word.
- **Syllable:** each note sings the next syllable, which is the closest to a singing robot.

Word and Syllable modes go back to the first word whenever your DAW starts playing, so loops line up every time.

**Hold vowels:** while a key is held, the last vowel keeps going (for example "hellooooo"). Let go and it finishes the word.

**Glide** slides between notes, **Vibrato** adds wobble, and pitch bend works over ±2 semitones.

## Controls

**Voice:** use the menu at the top to pick one of 14 personalities: Standard, Deep, Bright, Kid, Giant, Robot, Old Computer, Whisper, Breathy, Fast Talker, Singer, Choir Kid, Alien and Ghost. Each one sets the controls below, which you can then change.

- **Pitch:** the speaking pitch, used by Talk and Export (MIDI notes set their own pitch).
- **Speed:** how fast it talks.
- **Formant:** the size of the "head". Lower sounds bigger and deeper, higher sounds smaller and cartoonish.
- **Inflection:** how much the pitch rises and falls in Natural mode.
- **Intonation:** Natural (speech melody), Monotone (flat, classic robot), or Sung (steady pitch with vibrato).
- **Effort:** Normal, Breathy or Whispered.
- **Lo-fi:** lowers the sample rate and bit depth for that crunchy old-sound-card tone.
- **Volume:** output level.

## The display

The display shows the words, with the one being spoken highlighted, and that word's phonemes with the current sound lit. Below that are the output waveform and a vowel-space map. The map plots where the voice's mouth shape sits: "ee" is top left, "oo" top right and "ah" at the bottom.

## Credits

Pronunciations come from the [CMU Pronouncing Dictionary](https://github.com/cmusphinx/cmudict), Copyright (C) 1993-2015 Carnegie Mellon University, used under its BSD licence (see `Data/CMUDICT-LICENSE.txt`). Words not in the dictionary use letter-to-sound rules in the style of the public-domain Naval Research Laboratory rules (Elovitz et al., 1976).

## Building

GitHub builds it automatically whenever anything in this folder changes (see the Actions tab). To build it yourself with CMake and Visual Studio or Xcode, run this from this folder:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
