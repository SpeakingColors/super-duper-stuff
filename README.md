# HYPERSAWX 🐈

A JP-8000 style SuperSaw user oscillator for the **KORG minilogue xd**, free to use.

15 detuned saws, a sub oscillator, analog-style drift and vibrato.

It works now: the old ChatGPT version had several bugs (stereo writes into a mono buffer, a broken `_unit.c`, wrong manifest, no sources in the build). This version is a clean rewrite.

## Download and install

1. Download `dist/HyperSawX.mnlgxdunit`.
2. Open KORG **minilogue xd Sound Librarian**, connect the xd over USB.
3. Drag the file into a free **User Oscillator** slot, then send it to the synth.
4. On the xd: set **Multi Engine** to **USR** and pick **HyperSawX**.

## Controls

| Control | Function |
| --- | --- |
| **SHAPE** | Detune amount (JP-8000 curve) |
| **SHIFT + SHAPE** | Tone tilt (dark to bright) |
| **P1 Sub** | Sub oscillator, one octave down |
| **P2 Drift** | Slow random pitch drift per voice |
| **P3 Vib Depth** | Vibrato depth |
| **P4 Vib Rate** | Vibrato speed, 0.5 to 8 Hz (free running, not tempo synced; the SDK gives no tempo) |

Tip: lower **Multi Engine level** a little if you stack it with VCO 1 and 2.

## Listen

`audio/hypersawx_demo.wav` is a short riff rendered from this exact source.

## Build it yourself

Needs the [KORG logue-sdk](https://github.com/korginc/logue-sdk) and arm-none-eabi-gcc.

```
git clone --recursive https://github.com/korginc/logue-sdk
cp -r . logue-sdk/platform/minilogue-xd/hypersawx
cd logue-sdk/platform/minilogue-xd/hypersawx
make GCC_BIN_PATH=/path/to/arm-none-eabi/bin install
```

## Related

- [hypersaw-xd](https://github.com/SpeakingColors/hypersaw-xd): HyperSaw oscillator
- [sawwiden-xd](https://github.com/SpeakingColors/sawwiden-xd): stereo widener mod FX
- [MegaSaw-XD](https://github.com/SpeakingColors/MegaSaw-XD): 2 layer SuperSaw (coming)

## License

MIT for `hypersawx.cpp`. Files in `tpl/` and `ld/` come from the KORG logue SDK (BSD-3-Clause).

Not affiliated with or endorsed by KORG Inc. minilogue is a trademark of KORG Inc.
