# VerdaliScene TODO

Current as of 0.6.0: nine layer types, four of each, 4,484 parameters, nineteen
scenes, mixed by ear.

## The thing that has not been done

- [x] **Listen to the scenes.** Done in 0.6.0: the author re-mixed all nineteen
      by ear. What follows is why it was needed. Every fader in the factory scenes was set from a
      measured level and a target by role. That puts a layer where a number says
      it should be, not where it sounds right — a bird can measure exactly at
      −33.5 dBFS and still be too loud for a scene that is meant to be about the
      river. Sit through all nineteen, ideally against field recordings of the
      same kinds of place, and move what the ear disagrees with.

## Requested after the first listen (2026-10-04)

- [x] **A key starts and ends the scene.** Done in 0.2.0: *Notes* is the default
      gate and the factory scenes' setting, and the gate is every layer's drone
      note too, so each layer's own envelope plays inside the scene's. *Always*
      stays for a scene that should run with no key and no transport, and is
      what 0.1.0 projects carry; *Transport* stays for tying a scene to a song.
- [x] **Channel strips** with each layer's envelope (graph and knobs) and filter.
- [x] **Envelope curves** for the scene and every layer, bent in the graphs.
- [x] **Effects per channel** (0.3.0): reverb, delay, chorus, flanger, phaser,
      widener, auto-pan on every layer and on the scene.
- [x] **The cuckoo** (0.4.0): ChirpParade's tenth species reaches every birds
      layer, with its two presets and the *Cuckoo Wood* scene.
- [x] **Effect tails ring out or follow the release** (0.4.0): *FX Tails* on the
      scene's OUTPUT panel.

## Effects

- [ ] Listen to the reverb at its extremes -- two-minute decays, Size at both
      ends, Mod Depth at the top -- and to the delay past 100 % feedback, against
      a reference reverb and tape echo. Measured, not yet heard.
- [ ] Reorderable chains: the order is fixed today (modulation, delay, reverb,
      stereo).
- [ ] Shimmer: a +12 semitone pitch shifter in the reverb's or the delay's loop
      (DAFX 6.4.3), the one classic ambient effect not here yet.
- [ ] Dim a delay's Time or Note knob while the other one is the one in use.
- [ ] The effects view leaves the lower third of the page empty: room for a
      decay display or the delay's repeats drawn out.
- [x] **The factory scenes, mixed by ear** (0.6.0): taken from the author's pack
      as they came, `make_scenes.py` retired, demos re-rendered.

## Scenes

- [ ] More of them, and the kinds the factory set has none of: a city park, a
      greenhouse in rain, a ship's deck, a cave mouth with water inside.
- [ ] A scene preset kept across a host's preset change switches every layer's
      parameters at once. A short crossfade between the outgoing and incoming
      scene would make browsing scenes gentler.

## The window

- [ ] Undo for *REMOVE*: a layer's values are still in its slot until the slot
      is reused, so bringing the last removed layer back is cheap.
- [ ] A layer's own internal mixer (the plugin's *MIXER* overlay) on its page.
      The base window can already draw it; it needs its mute/solo holds released
      on every page switch.
- [ ] Layers in the order they were added rather than by kind.

## Engine

- [ ] A per-layer intensity: the drone note's velocity. Several plugins scale
      density or distance with velocity, so it would be a useful macro — but it is
      read at note-on, and changing it means restriking the drone.
- [ ] A shared space for the whole scene, as an option over each layer's own.

## Suite

- [ ] Envelope curves in the nine plugins themselves. Their engines take them
      already; each needs three parameters appended and a home on its ENVELOPE
      panel, which changes every plugin's window and its website screenshot.

- [ ] The VST3 class ids are still the wrapper's hash of the CLAP id. Pin them
      with `CLAP_VST3_TUID_STRING` before 1.0 (see the suite's *Still open*).
- [ ] The CLAP preset-discovery factory does not cross into a VST3 host's own
      browser; the window's browser lists everything regardless.
