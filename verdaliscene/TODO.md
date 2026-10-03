# VerdaliScene TODO

Current as of 0.1.0: nine layer types, four of each, 2,119 parameters, sixteen
scenes.

## The thing that has not been done

- [ ] **Listen to the scenes.** Every fader in the factory scenes was set from a
      measured level and a target by role. That puts a layer where a number says
      it should be, not where it sounds right — a bird can measure exactly at
      −33.5 dBFS and still be too loud for a scene that is meant to be about the
      river. Sit through all sixteen, ideally against field recordings of the
      same kinds of place, and move what the ear disagrees with.

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

- [ ] The VST3 class ids are still the wrapper's hash of the CLAP id. Pin them
      with `CLAP_VST3_TUID_STRING` before 1.0 (see the suite's *Still open*).
- [ ] The CLAP preset-discovery factory does not cross into a VST3 host's own
      browser; the window's browser lists everything regardless.
