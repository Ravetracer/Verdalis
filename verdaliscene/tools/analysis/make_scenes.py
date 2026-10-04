#!/usr/bin/env python3
"""Writes the factory scenes from one table.

A scene is a list of layers, each a plugin preset by name and a role. The role
says how loud the layer should sit -- the bed a scene is built on, a texture
over it, an accent heard now and then -- and the fader is whatever brings the
preset there, worked out from the preset's level measured alone at 0 dB:

    verdaliscene-render --preset <scene> --layers

prints every layer's level solo. levels.tsv holds those measurements for every
preset a scene here uses (plugin, preset, rms dBFS, peak dBFS), taken over 30 s,
90 s for thunder. Thunder is placed by its peaks, because a storm is heard by
its flashes and its RMS says more about how often they come than how loud
they are.

A role can also be a number, which is the fader itself. That is for the layers
neither measurement places well: a bird that only sings now and then (one or
two phrases in a probe say little about its level, so those were set from their
peaks over a long render of the finished scene), and a layer whose transients
reach the ceiling at the level its RMS asks for.

    python3 make_scenes.py levels.tsv ../../presets
"""

import sys
from pathlib import Path

# Target levels by role, dBFS. The bed sits where a single plugin's preset
# does; everything else is placed against it.
ROLE_RMS = {'bed': -27.0, 'second': -30.5, 'texture': -33.5, 'accent': -37.0, 'faint': -40.0}
ROLE_PEAK = {'storm': -5.0, 'thunder': -8.0, 'distant': -12.0}

PLUGIN = {'rain': 'RainyDay', 'thunder': 'ThunderClap', 'waves': 'ShoreBreak', 'wind': 'SkyHowl',
          'birds': 'ChirpParade', 'river': 'RiverFlow', 'fire': 'CrackleBlaze',
          'insects': 'InsectSwarm', 'night': 'NightLife'}

ENVELOPE = '''# Envelope
gate = Notes
attack = {attack}
decay = 1000
sustain = 1
release = {release}

# Filter
filter_type = Lowpass
highpass = {highpass}
filter_cutoff = {cutoff}
filter_reso = 0.1

# Output
width = {width}
gain = {gain}
tails = {tails}
'''

# A big, dark stone space for the cave's layers: long, damped hard above 3.5 kHz,
# with the rumble kept out so the drips stay clear.
CAVE_REVERB = ['fx_reverb_on = On', 'fx_reverb_size = 0.85', 'fx_reverb_diffusion = 0.8',
               'fx_reverb_damping = 0.55', 'fx_reverb_damp_freq = 3500',
               'fx_reverb_mod_depth = 0.4', 'fx_reverb_low_cut = 140',
               'fx_reverb_high_cut = 7000']

# name, file, description, features, scene settings, layers.
# A layer is (kind, preset, role, extra lines). Extra lines are the plugin's
# own keys or layer_ ones, written into the section as they are.
SCENES = [
    ('Forest River', 'forest_river',
     'A stream running through a summer wood: birds in the canopy, wind moving the leaves '
     'overhead, and bees working the bank.',
     'ambient, nature, forest, water, birds, day', {},
     [('river', 'Forest Stream', 'bed', []),
      ('birds', 'Forest Morning', 'texture', []),
      ('wind', 'Wind In Birches', 'accent', []),
      ('insects', 'Bumblebee Garden', 'faint', ['layer_pan = 0.3'])]),

    ('Stormy Shore', 'stormy_shore',
     'An ocean shore in a thunderstorm: heavy surf, rain sweeping in off the sea, wind tearing '
     'along the beach and thunder rolling out over the water.',
     'ambient, nature, sea, rain, storm, thunder, wind', {'attack': 3000},
     [('waves', 'Big Waves Crashing', 'bed', []),
      ('rain', 'Storm Front', 'second', []),
      ('wind', 'Howling Storm', 'texture', []),
      ('thunder', 'Rolling Thunder', 'storm', ['storm_rate = 3'])]),

    ('Campfire Night', 'campfire_night',
     'A fire burning down in a clearing at night: crickets in the grass, an owl somewhere in the '
     'trees and the faintest breath of wind.',
     'ambient, nature, fire, night, owls, insects', {},
     [('fire', 'Forest Campfire', 2.5, []),
      ('night', 'Summer Night', 'second', []),
      ('night', 'Tawny Wood', 'accent', ['layer_stereo = Mono', 'layer_pan = -0.55']),
      ('wind', 'Soft Breath', 'faint', [])]),

    ('Rain on the Cabin', 'rain_on_the_cabin',
     'Rain drumming on a tin roof while the stove ticks over inside, wind worrying at the '
     'corners and a storm grumbling somewhere beyond the valley.',
     'ambient, nature, rain, fire, indoor, thunder', {},
     [('rain', 'Tin Roof', 'bed', []),
      ('fire', 'Wood Stove', 'accent', ['layer_pan = -0.25']),
      ('wind', 'Between Houses', 'faint', []),
      ('thunder', 'Distant Rumble', 'distant', ['storm_rate = 2'])]),

    ('Summer Meadow', 'summer_meadow',
     'Midday in open grassland: crickets and bees in the grass, sparrows in the hedge and a warm '
     'breeze moving through it all.',
     'ambient, nature, meadow, insects, birds, wind, day', {},
     [('insects', 'Summer Meadow', 'bed', []),
      ('birds', 'Garden Sparrows', 'texture', []),
      ('wind', 'Meadow Breeze', 'texture', []),
      ('insects', 'Honeybee Field', 'faint', ['layer_pan = -0.35'])]),

    ('Monsoon Jungle', 'monsoon_jungle',
     'A tropical forest under a monsoon downpour: thunder overhead, screeching birds in the '
     'canopy and cicadas that never stop.',
     'ambient, nature, rain, thunder, jungle, birds, insects', {'attack': 3000},
     [('rain', 'Tropical Monsoon', 'bed', []),
      ('birds', 'Jungle Screech', 'texture', []),
      ('insects', 'Cicada Noon', 'accent', []),
      ('thunder', 'Under The Rain', 'thunder', ['storm_rate = 3'])]),

    ('Mountain Stream', 'mountain_stream',
     'High up, where a cold stream drops between boulders: wind along the ridge and a few birds '
     'on the tree line far below.',
     'ambient, nature, mountain, water, wind, birds', {},
     [('river', 'Mountain River', 'bed', []),
      ('wind', 'Mountain Ridge', 'texture', []),
      ('birds', 'Far Treeline', 'faint', [])]),

    ('Frog Pond', 'frog_pond',
     'A pond at dusk: a chorus of frogs, spring peepers in the reeds, a little water bubbling in '
     'at one end and mosquitoes over the surface.',
     'ambient, nature, night, frogs, water, insects, dusk', {},
     [('night', 'Pond Chorus', 'bed', []),
      ('night', 'Spring Peepers', 'texture', ['layer_pan = 0.3']),
      ('river', 'Bubbling Creek', 'accent', ['layer_stereo = Mono', 'layer_pan = -0.5']),
      ('insects', 'Mosquito Night', 'faint', [])]),

    ('Winter Cabin', 'winter_cabin',
     'A gale and driving snow outside, a hearth glowing in the room, and the river below the '
     'cabin slowed to a murmur under the ice.',
     'ambient, nature, winter, wind, fire, indoor', {'attack': 4000},
     [('wind', 'Winter Gale', 'bed', []),
      ('fire', 'Hearth Glow', 'second', ['layer_pan = 0.2']),
      ('wind', 'Snow Storm', 'texture', []),
      ('river', 'Winter River', 'faint', [])]),

    ('Harbour Drizzle', 'harbour_drizzle',
     'A small harbour on a grey morning: water lapping at the stones, a fine drizzle, and the '
     'wind in the grass along the quay.',
     'ambient, nature, sea, rain, wind, morning', {},
     [('waves', 'Harbour Lapping', 'bed', []),
      ('rain', 'Light Drizzle', 'second', []),
      ('wind', 'Beach Grass', 'accent', [])]),

    ('Distant Storm', 'distant_storm',
     'A storm passing far off across open country: a wall of rain on the horizon, thunder '
     'rolling in from kilometres away and wind off the plain.',
     'ambient, nature, rain, thunder, wind, distant', {'cutoff': 9000, 'attack': 4000},
     [('rain', 'Distant Rain Wall', 'bed', []),
      ('wind', 'Far Away', 'second', []),
      ('thunder', 'Far Horizon', 'thunder', ['storm_rate = 2'])]),

    ('Wolf Valley', 'wolf_valley',
     'Moonlight over a valley: wolves answering each other from slope to slope, a river far '
     'below and a cold wind coming down off the tops.',
     'ambient, nature, night, wolves, water, wind', {},
     [('night', 'Wolf Valley', -5.0, []),
      ('river', 'Distant River', 'texture', []),
      ('wind', 'Spooky Wind', 'texture', [])]),

    ('Waterfall Clearing', 'waterfall_clearing',
     'A clearing beside a waterfall on a spring morning: the roar of the fall, a dawn chorus in '
     'the trees around it and dragonflies over the pool.',
     'ambient, nature, water, birds, insects, morning', {},
     [('river', 'Big Falls', 'bed', []),
      ('birds', 'Dawn Chorus', 'texture', []),
      ('insects', 'Dragonfly Pond', 'faint', ['layer_pan = 0.4'])]),

    ('Beach Bonfire', 'beach_bonfire',
     'A bonfire on the sand on a warm night: small waves breaking on the beach, crickets in the '
     'dunes and a soft wind off the sea.',
     'ambient, nature, fire, sea, night, insects', {},
     [('fire', 'Bonfire', 'bed', ['layer_pan = -0.15']),
      ('waves', 'Gentle Waves', 'second', []),
      ('night', 'Cricket Field', 'accent', []),
      ('wind', 'Beach Grass', 'faint', [])]),

    ('Rain in the Woods', 'rain_in_the_woods',
     'Steady rain on the forest canopy, a swollen creek running between the trees, a robin '
     'singing through it and thunder now and then.',
     'ambient, nature, rain, forest, water, birds, thunder', {},
     [('rain', 'Rain On Leaves', 0.0, []),
      ('river', 'Forest Creek', 'second', []),
      ('birds', 'Robin Whistle', -1.0, ['layer_stereo = Mono', 'layer_pan = 0.45',
                                         'layer_shot_rate = 3']),
      ('thunder', 'Distant Rumble', -7.0, ['storm_rate = 2'])]),

    ('Dawn Chorus', 'dawn_chorus',
     'The hour before sunrise in a wood: the chorus building, a blackbird and a robin singing '
     'close by, a woodpecker drumming further off.',
     'ambient, nature, birds, forest, morning', {'attack': 4000},
     [('birds', 'Dawn Chorus', 'bed', []),
      ('birds', 'Blackbird Song', -0.5, ['layer_stereo = Mono', 'layer_pan = -0.35',
                                          'layer_shot_rate = 4']),
      ('birds', 'Robin Whistle', -1.5, ['layer_stereo = Mono', 'layer_pan = 0.4',
                                         'layer_shot_rate = 3']),
      ('birds', 'Woodpecker Drum', 2.5, ['layer_stereo = Mono', 'layer_pan = 0.7']),
      ('wind', 'Soft Breath', 'faint', [])]),

    # The two that show off the layers' effects. Inside the cave every sound
    # carries the cave with it; the wind at its mouth is outside and dry. Their
    # numeric faders were set from --layers renders of the finished scenes,
    # because a reverb adds to a layer's level what its solo measurement does
    # not know about.
    ('Cave Mouth', 'cave_mouth',
     'Just inside the mouth of a cave: water dripping into pools in the dark, a trickle running '
     'down the rock, and the wind outside moaning across the entrance.',
     'ambient, nature, cave, water, wind, reverb', {'attack': 3000, 'release': 6000},
     [('rain', 'Cave Drips', 2.0, CAVE_REVERB + ['fx_reverb_decay = 7000',
                                                   'fx_reverb_predelay = 35',
                                                   'fx_reverb_mix = 0.5']),
      ('river', 'Hanging Trickle', 7.5, CAVE_REVERB + ['fx_reverb_decay = 6000',
                                                            'fx_reverb_predelay = 22',
                                                            'fx_reverb_mix = 0.4',
                                                            'layer_pan = 0.35']),
      ('wind', 'Cave Mouth', 'texture', [])]),

    ('Canyon Echo', 'canyon_echo',
     'A bird calling across a desert canyon and its song coming back off the far wall, wind '
     'wandering along the rim, and a river far below.',
     'ambient, nature, canyon, birds, wind, water, delay, reverb', {'attack': 3000},
     [('wind', 'Mountain Ridge', 2.0, ['fx_autopan_on = On', 'fx_autopan_rate = 0.05',
                                         'fx_autopan_depth = 0.4', 'fx_autopan_shape = Drift']),
      ('birds', 'Melody Bird', 'texture', ['layer_stereo = Mono', 'layer_pan = -0.45',
                                           'layer_shot_rate = 4',
                                           'fx_delay_on = On', 'fx_delay_mode = Ping-Pong',
                                           'fx_delay_time = 430', 'fx_delay_offset = 0.3',
                                           'fx_delay_feedback = 0.5', 'fx_delay_low_cut = 250',
                                           'fx_delay_high_cut = 5000',
                                           'fx_delay_saturation = 0.1',
                                           'fx_delay_diffusion = 0.25', 'fx_delay_mix = 0.4',
                                           'fx_reverb_on = On', 'fx_reverb_mix = 0.3',
                                           'fx_reverb_decay = 4500', 'fx_reverb_size = 0.9',
                                           'fx_reverb_predelay = 60', 'fx_reverb_low_cut = 200']),
      ('river', 'Distant River', 'faint', ['fx_reverb_on = On', 'fx_reverb_mix = 0.35',
                                           'fx_reverb_decay = 3500', 'fx_reverb_size = 0.7',
                                           'fx_reverb_high_cut = 5000'])]),

    # A cuckoo close by, and a pair answering from across the valley through a
    # long reverb: the valley is the space the call carries in, which is why
    # the far pair are wetter than the near one rather than only quieter.
    # Cuckoo Valley's level in levels.tsv is taken through that reverb.
    ('Cuckoo Wood', 'cuckoo_wood',
     'A beech wood in late spring: a cuckoo calling from the trees close by, two more answering '
     'from across the valley, small birds in the canopy and a breeze in the leaves.',
     'ambient, nature, birds, forest, spring, cuckoo, reverb', {'attack': 3000, 'release': 5000},
     [('birds', 'Garden Sparrows', 'faint', []),
      ('birds', 'Cuckoo Call', 'second', ['layer_stereo = Mono', 'layer_pan = -0.3',
                                     'layer_shot_rate = 1']),
      ('birds', 'Cuckoo Valley', 'accent', ['layer_pan = 0.3', 'layer_shot_rate = 0',
                                       'fx_reverb_on = On', 'fx_reverb_decay = 4500',
                                       'fx_reverb_size = 0.9', 'fx_reverb_predelay = 70',
                                       'fx_reverb_low_cut = 250', 'fx_reverb_high_cut = 4500',
                                       'fx_reverb_mix = 0.35']),
      ('wind', 'Wind In Birches', 'texture', [])]),
]


def main():
    levels = {}
    for line in Path(sys.argv[1]).read_text().splitlines():
        plugin, preset, rms, peak = line.split('\t')
        levels[(plugin, preset)] = (float(rms), float(peak))
    out = Path(sys.argv[2])
    for name, stem, desc, features, settings, layers in SCENES:
        s = {'attack': 2500, 'release': 4000, 'highpass': 20, 'cutoff': 20000, 'width': 1,
             'gain': 0, 'tails': 'Ring Out'}
        s.update(settings)
        text = '# VerdaliScene preset\nformat = 1\nname = %s\nauthor = VerdaliScene\n' % name
        text += 'description = %s\nfeatures = %s\n\n' % (desc, features)
        text += ENVELOPE.format(**s)
        for kind, preset, role, extra in layers:
            plugin = PLUGIN[kind]
            rms, peak = levels[(plugin, preset)]
            if isinstance(role, (int, float)):
                fader = float(role)
            elif role in ROLE_PEAK:
                fader = ROLE_PEAK[role] - peak
            else:
                fader = ROLE_RMS[role] - rms
            fader = max(-30.0, min(12.0, round(fader * 2) / 2))
            text += '\nlayer = %s\nlayer_from = %s\nlayer_level = %g\n' % (plugin, preset, fader)
            for line in extra:
                text += line + '\n'
        (out / (stem + '.verdaliscene')).write_text(text)
        print('%-20s %d layers' % (name, len(layers)))


if __name__ == '__main__':
    main()
