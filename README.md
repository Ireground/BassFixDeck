# BassFixDeck

A VST3 that keeps a bass line even across its whole range. It tracks the pitch of each bass note and:

- **cuts the boom** on low notes: below *Boom Below*, it cuts a bell that follows the fundamental, reaching the full *Boom Cut* depth *Range* semitones lower.
- **restores body** on high notes: above *Body Above*, it boosts the fundamental, reaching the full *Body Boost* *Range* semitones higher.

Drag the orange (boom) and teal (body) handles in the display, or use the knobs. Double-click a handle to reset it.

| Control | What it does |
|---|---|
| Boom Below / Boom Cut | Note where the boom cut starts / maximum cut |
| Body Above / Body Boost | Note where the body boost starts / maximum boost |
| Range | How many semitones it takes to reach the full cut or boost |
| Focus | Width (Q) of the tracking bell. Higher = only the fundamental |
| Harmonics | Adds upper harmonics on low notes in place of the cut boom, so they stay audible on small speakers |
| Speed | How fast the correction follows each note |
| Mix / Output | Dry/wet and output level |
| Lookahead | Delays audio 50 ms so corrections hit right on the note start (REAPER compensates automatically) |
| Delta | Listen only to what the plugin changes |

Built for monophonic bass (synth bass, bass guitar, 808s). Chords make the pitch reading unreliable; when no clear note is found, the plugin eases back to neutral.

## Build
GitHub Actions builds it automatically on every push (Actions tab → latest run → *BassFixDeck-VST3-Windows*). Copy `BassFixDeck.vst3` to `C:\Program Files\Common Files\VST3`.
