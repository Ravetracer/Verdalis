# Verdalis website

The static website for the Verdalis suite and the other Ravetracer plugins:
plain HTML, CSS and JavaScript, no backend, no database, no external requests
except one to the GitHub API for the download links.

```sh
node website/tools/build.mjs            # -> dist-website/
node website/tools/serve.mjs 8080       # preview at http://localhost:8080
```

Upload the contents of `dist-website/` (including `.htaccess`) to the web
space. Use `serve.mjs` for previews rather than `python -m http.server`: it
supports HTTP Range requests, which seeking in a FLAC needs.

## Downloads are resolved at page load

No download link is built into the page. `site/assets/js/releases.js` asks
GitHub for the newest release when a visitor opens a page:

- **Verdalis**: `Ravetracer/Verdalis` `releases/latest`, picking the
  `-linux-x86_64.zip`, `-windows-x86_64.zip`, `-macos-universal.zip` and (once
  there is one) `.wclap.tar.gz` assets.
- **Other plugins**: `Ravetracer/audio-plugins` publishes one release per
  plugin, tagged `<name>-v<version>`. The page takes the highest version with the
  plugin's tag prefix (`release.tag` in `content/plugins.json`) and its
  `<Name>-<version>.zip` (Linux and Windows), `-macOS.zip`, `.wclap.tar.gz` and
  `-Manual.pdf`.

A new release therefore needs no change to the site at all. Answers are cached
in the visitor's browser for 30 minutes (GitHub allows 60 anonymous API calls
per hour and visitor), and if GitHub cannot be reached every button still
links to the release page. A platform button only appears when the release has
a file for it, so a Verdalis WebCLAP button shows up by itself the day the
suite release carries a `.wclap.tar.gz`.

## Layout

```
website/
├── site/                 the page: index.html, imprint.html, privacy.html,
│                         .htaccess, site.json, assets/
│   └── assets/js/        app.js (router), views.js (pages), releases.js,
│                         icons.js; player/ui/waveform/visualizer/util/config
│                         come from the NGC 224 music site's player
├── content/plugins.json  every text on the site, per plugin
├── images/               current window screenshots of every plugin
├── demos/<slug>/         FLAC demos + manifest.json   (gitignored, rendered)
├── tools/
│   ├── build.mjs                 assembles dist-website/
│   ├── serve.mjs                 local preview server
│   ├── render-verdalis-demos.py  renders the Verdalis demos
│   ├── render-ap-demos.sh        renders the audio-plugins demos
│   ├── capture-verdalis-screenshots.sh   Verdalis windows, preset loaded
│   └── capture-ap-screenshots.sh         audio-plugins windows (read-only on
│                                         that repo, out-of-tree builds)
└── .cache/               waveform peaks, keyed by audio MD5 (gitignored)
```

## Demos

At most ten per plugin, FLAC, 48 kHz / 16 bit, one linear gain towards
-16 LUFS with the true peak kept at or below -1 dBTP. No compression, so a quiet
preset stays quiet.

```sh
website/tools/render-verdalis-demos.py [slug ...]    # all plugins built in <plugin>/build
website/tools/render-ap-demos.sh                     # see the script's header
node website/tools/build.mjs
```

Which presets are rendered is the list at the top of
`render-verdalis-demos.py`. The text under each demo comes from the plugin's
`presets/demo-descriptions.txt` (Verdalis) or from `demoText` in
`content/plugins.json`, keyed by file name, which overrides either.

## Content

`content/plugins.json` holds, per plugin: the wordmark (two parts, the second in
the accent colour), the accent (the plugin window's own), an icon from
`site/assets/js/icons.js`, tagline, card text, intro, three highlights,
screenshots with alt text, and for the non-Verdalis plugins the release tag
prefix, source link and any trademark notice. The cover art is generated from
the icon and accent by `build.mjs`.

Old WordPress permalinks (`/uncategorized/<slug>/`, `/non-verdalis/<slug>/`,
`/download/...`) are redirected by `.htaccess`; a plugin's `aliases` list the
old slugs.

## Screenshots

```sh
website/tools/capture-verdalis-screenshots.sh [folder ...]
website/tools/capture-ap-screenshots.sh <audio-plugins-dir> <build-dir>
```

Both open the real windows on a private Xvfb display. The Verdalis script loads
one characteristic preset per plugin (the table at its top) and clicks through
VerdaliScene's layer and effects pages; the other runs each plugin's own
`make-screenshots.sh` from a patched copy so nothing is written into that
repository. The plugins' guihost sometimes loses an X race on startup
(`BadWindow`); the script retries once.

## Legal pages

`site/imprint.html` and `site/privacy.html` are plain HTML, reachable without
JavaScript, German first (binding) and an English translation below. The
privacy policy describes what the site actually does: web hosting and server
logs at Alfahosting, e-mail, the GitHub API request for the download links and
the downloads themselves, and the browser's local storage (player volume and
position, cached release data). Update it whenever the site starts doing
something new, e.g. a font or script from another server.
