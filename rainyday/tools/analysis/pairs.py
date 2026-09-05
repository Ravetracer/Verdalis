"""Which reference recording each preset is trying to be.

The recordings themselves are not in the repository -- point --sounds at your
own directory of WAVs named after the keys used here (mono or stereo, any
of the PCM depths wavio reads; stereo is summed to mono on load). Presets
listed with TONE keep the density and rhythm that give them their identity and
only have their spectral character corrected; there is no recording of a
tropical monsoon in the reference set, but its tone should still follow real
rain.

A name that appears in refs.COMPOSITES is a class of recordings averaged
together rather than a single file, so the preset is fitted to what that kind of
rain measures like instead of to one microphone in one room on one afternoon.

Window Pane and Gutter Trickle were both matched to the nearest neighbour in the
old set for want of anything better, and now have recordings of their own: eleven
of rain on glass and three of water running down a drain. Both move from TONE to
FULL with them, because a recording of the thing itself is worth following in
rhythm and density and not only in tone.

Distant Rain Wall is deliberately absent. Nothing in the reference set is a
far-field recording, and fitting it to a close one inverts what it is: the fit
strips out the far-field wash that is the whole point of the preset. It is
built instead from the fitted Downpour with Distance and Air Absorption pushed
up, which is what "the same rain, further away" actually means.
"""

FULL = 'full'
TONE = 'tone'
# Sparse drips live or die on the single droplet and the room around it, and
# where the recording is of that room, the space and filter controls are fitted
# too. The cave reference has a measured reverberation time and a measured slap;
# leaving Space Size to taste would be leaving the cave to taste.
DROP = 'drop'

PAIRS = [
    ('steady_rain',       'rain_on_roof',                FULL),
    ('rain_on_leaves',    'rain_on_leafes',              FULL),
    ('concrete_alley',    'rain_on_concrete',            FULL),
    ('tin_roof',          'rain_on_metal',               FULL),
    ('light_drizzle',     'rain_soft',                   FULL),
    ('inside_the_car',    'rain_in_car',                 FULL),
    ('puddle_plinks',     'multiple_water_drops',        FULL),
    ('dripping_faucet',   'multiple_water_drops_faucet', FULL),
    ('cave_drips',        'long_real_cave_drops',        DROP),
    ('downpour',          'rain_on_concrete',            TONE),
    ('storm_front',       'rain_on_concrete',            TONE),
    ('tropical_monsoon',  'rain_on_roof',                TONE),
    ('first_drops',       'rain_on_concrete',            TONE),
    ('gutter_trickle',    'sewer',                       FULL),
    ('window_pane',       'window',                      FULL),
    ('under_an_umbrella', 'umbrella',                    FULL),
]

# Below 100 Hz a field recording is mostly wind and handling noise, which
# RainyDay does not model, so that band barely counts. The top band is half
# weighted because it is where recording chains differ most.
BAND_WEIGHT = [0.25, 0.6, 1.0, 1.0, 1.0, 1.0, 1.0, 0.9, 0.5]
