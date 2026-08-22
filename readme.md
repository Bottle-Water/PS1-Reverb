# Ps1Verb

The PlayStation 1 SPU reverb as a VST3/AU plugin, in C++ with JUCE.

Built from nocash's SPU notes,
[reverb formula](https://problemkaputt.de/psxspx-spu-reverb-formula.htm),
[reverb examples](https://problemkaputt.de/psxspx-spu-reverb-examples.htm), and
[psx-spx](https://psx-spx.consoledev.net/soundprocessingunitspu/).


<img src="PS1Verb.png" alt="Ps1Verb" width="420">

Additionally to the PS1 base reverb, this also allows you to mix the input and output levels, as well as stereo width, pre-delay, and dampening.

## Presets

Taken from the SPU registers:

`Room`, `Studio Small`, `Studio Medium`, `Studio Large`, `Hall`,
`Half Echo`, `Space Echo`, `Chaos Echo`, `Delay`

## Demo

Dry, then the same source through `Hall`:

https://github.com/user-attachments/assets/d9ac4d21-672c-495a-b39b-1d8a6a5b2b5c
## Technicals
The core is in integer Q15 format, running the reverb algorithm at the SPU's half rate, accurately to the original PS1 SPU. The preset tables were verified against register dumps from a running emulator. It was also found that Wipeout tunnels use the "Hall" preset through these register dumps.


## Build

Needs [JUCE](https://github.com/juce-framework/JUCE). The `.jucer` uses
Projucer's global module path, so either clone JUCE to `~/JUCE` or set
**Projucer -> Settings -> Global Paths -> JUCE Modules** to the install location.

Open `Ps1Verb.jucer` in Projucer, save to generate the build files, then build.

