"""Which reference recording each preset is trying to be.

The recordings themselves are not in the repository -- point --sounds at your
own directory of mono WAVs named after the keys used here. Presets listed with
TONE keep the density and rhythm that give them their identity and only have
their spectral character corrected; there is no recording of a tropical monsoon
in the reference set, but its tone should still follow real rain.

Distant Rain Wall is deliberately absent. Nothing in the reference set is a
far-field recording, and fitting it to a close one inverts what it is: the fit
strips out the far-field wash that is the whole point of the preset. It is
built instead from the fitted Downpour with Distance and Air Absorption pushed
up, which is what "the same rain, further away" actually means.
"""

FULL = 'full'
TONE = 'tone'

PAIRS = [
    ('steady_rain',       'rain_on_roof',                FULL),
    ('rain_on_leaves',    'rain_on_leafes',              FULL),
    ('concrete_alley',    'rain_on_concrete',            FULL),
    ('tin_roof',          'rain_on_metal',               FULL),
    ('light_drizzle',     'rain_soft',                   FULL),
    ('inside_the_car',    'rain_in_car',                 FULL),
    ('puddle_plinks',     'multiple_water_drops',        FULL),
    ('dripping_faucet',   'multiple_water_drops_faucet', FULL),
    ('cave_drips',        'water_drops_cave_reverb',     FULL),
    ('downpour',          'rain_on_concrete',            TONE),
    ('storm_front',       'rain_on_concrete',            TONE),
    ('tropical_monsoon',  'rain_on_roof',                TONE),
    ('first_drops',       'rain_on_concrete',            TONE),
    ('gutter_trickle',    'multiple_water_drops',        TONE),
    ('window_pane',       'rain_in_car',                 TONE),
]

# Below 100 Hz a field recording is mostly wind and handling noise, which
# RainyDay does not model, so that band barely counts. The top band is half
# weighted because it is where recording chains differ most.
BAND_WEIGHT = [0.25, 0.6, 1.0, 1.0, 1.0, 1.0, 1.0, 0.9, 0.5]
