#!/usr/bin/env python3
"""Fetch reference recordings from Fraser Simpson's sonogram library.

    python3 fetch-fss.py --list Raven Goose      # what is available, no download
    python3 fetch-fss.py Raven Goose Crow        # fetch those species
    python3 fetch-fss.py --all                   # every species in PAGES
    python3 fetch-fss.py --page ravensonogram    # one site page, by its slug

https://www.fssbirding.org.uk/sonagrams.htm holds 515 species pages. Each pairs
a sonogram image with the recording it was made from, and it is the *recording*
this script wants: the images are 790x387 for seven seconds, about 9.5 ms per
pixel, which is thirty times coarser than the 0.33 ms hop contours.py tracks at.
Tracing them would reintroduce exactly the smoothing that version 0.2.0 was
written to get rid of. The mp3 beside each image does not have that problem.

The library ChirpParade is fitted to is thin in two places -- three Goose
contours and four Raven, because only one recording of each exists in it -- and
this site carries roughly twelve corvid and five waterfowl recordings. That is
what this is for.

## Provenance and licence

The site is marked "Copyright Fraser Simpson, all rights reserved". It is not a
Creative Commons library, and nothing fetched here may be redistributed. The
files land in !dev/, which is gitignored, and are treated exactly like the rest
of the reference library: read, never copied, never shipped. What ships is
measured coefficients, which the suite's synthesis rule allows and which are
not the recording.

Fraser Simpson also uploads to Xeno-Canto, where each recording carries an
explicit Creative Commons licence. Where the same cut exists there, that is the
better provenance and worth preferring.

Every download is recorded in PROVENANCE.tsv next to the files: source URL, the
species/location/date the mp3 carries in its ID3 tags, its copyright line, the
sha256 of the mp3 as fetched, and when it was fetched.

## What it writes

mp3 as fetched, plus a 16-bit PCM wav decoded from it at the source rate, named

    fss_<species>_<page>_<id>.wav

so that species.py's prefix table groups it without a filename having to lie
about what is in it. The wav is what the analysis tools read -- they take .wav
alone, and wavio reads PCM only.

The default output directory is !dev/references/fss/, which is *beside* the
reference library rather than in it: contours.py lists REFS without recursing,
so a fetch changes no existing measurement until the files are moved up
deliberately. The script prints that command when it finishes.

## The one caveat that matters for measurement

These are 128 kbps mp3s. That is transparent enough for a contour, which lives
below 12 kHz and is a ridge rather than a texture, but the codec's own noise
shaping sits in the same place as the partial balance above about 5 kHz. Trust
contours from this material; be sceptical of what it says about Partials, and
prefer the existing library for that.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.request

SITE = "https://www.fssbirding.org.uk"
INDEX = SITE + "/sonagrams.htm"
UA = "ChirpParade-reference-fetch/1.0 (+https://github.com/Ravetracer/Verdalis)"

HERE = os.path.dirname(os.path.abspath(__file__))
OUTDIR = os.environ.get(
    "CHIRPPARADE_FSS",
    os.path.normpath(os.path.join(HERE, "../../!dev/references/fss")))

# A courtesy delay between requests. The site is one person's, served from
# shared hosting, and there is no hurry here.
DELAY_SEC = 1.5

# Which site pages feed which ChirpParade species. The species names are the
# engine's own; the slugs are the site's page names without the .htm.
#
# Only species the engine models are listed, and only pages whose bird is the
# same *voice*, not merely the same family -- a Jackdaw and a Raven are both
# corvids and sound nothing alike, but both are rough, low and broadband, which
# is what the Crow and Raven archetype sets are short of. Chough and Nutcracker
# are deliberately absent: they are corvids with whistled calls.
PAGES = [
    ("Raven",      ["ravensonogram"]),
    ("Crow",       ["carrioncrowsonogram", "hoodedcrowsonogram",
                    "rooksonogram", "jackdawsonogram", "americancrowsonogram"]),
    ("Goose",      ["canadagoosesonogram", "greylagsonogram",
                    "barnaclegoosesonogram", "beangoosesonogram",
                    "snowgoosesonogram"]),
    ("Crane",      ["cranesonogram", "sandhillcranesonogram"]),
    ("Whistler",   ["robinsonogram"]),
    ("Warbler",    ["nightingalesonogram", "thrushnightingalesonogram"]),
    ("Woodpecker", ["greenwoodpeckersonogram", "blackwoodpeckersonogram",
                    "middlespottedwoodpeckersonogram"]),
]

SPECIES_ORDER = [name for name, _ in PAGES]

MP3_RE = re.compile(rb'(?:src|href)\s*=\s*"([^"]*/Sounds/[^"]+\.mp3)"', re.I)


def get(url, referer=None):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    if referer:
        req.add_header("Referer", referer)
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def page_url(slug):
    return "%s/%s.htm" % (SITE, slug)


def sounds_on(slug):
    """The mp3s a species page links, in page order, without duplicates.

    The site writes each recording twice, once as the audio element's src and
    once as the download link, and some pages give the URL absolute and some
    relative. Both are normalised here, and http is raised to https: the server
    answers on both and there is no reason to fetch the plain one.
    """
    html = get(page_url(slug))
    out = []
    for m in MP3_RE.findall(html):
        u = m.decode("latin-1")
        if u.startswith("http://"):
            u = "https://" + u[len("http://"):]
        elif not u.startswith("https://"):
            u = "%s/%s" % (SITE, u.lstrip("/"))
        if u not in out:
            out.append(u)
    return out


def id3_tags(path):
    """Species, location and date, which the site puts in the mp3 itself.

    ffprobe rather than a tag parser: it is already a dependency of the decode
    step below, and the tags here are ID3v2.3 with a Latin-1 comment that a
    minimal parser gets wrong.
    """
    try:
        out = subprocess.run(
            ["ffprobe", "-v", "quiet", "-show_entries",
             "format_tags=artist,title,comment,date", "-of", "default=nw=1",
             path], capture_output=True, text=True, timeout=30).stdout
    except (OSError, subprocess.SubprocessError):
        return {}
    tags = {}
    for line in out.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            tags[k.split(":")[-1].strip().lower()] = v.strip()
    return tags


def decode(mp3, wav):
    """mp3 -> 16-bit PCM wav at the source rate.

    The rate and the channel count are left alone. wavio reads PCM only, and
    the analysis tools mono the file down themselves, so there is nothing to
    decide here -- resampling would only throw away what the segmenter uses.
    """
    subprocess.run(
        ["ffmpeg", "-v", "error", "-y", "-i", mp3, "-c:a", "pcm_s16le", wav],
        check=True, timeout=300)


def stem(species, slug, url):
    """fss_<species>_<page>_<id>, the name species.py can group on.

    The id is the site's own filename with the species part stripped, because
    it already encodes the recording -- carrioncrow115041e.mp3 is one cut and
    carrioncrowLS111948ecut.mp3 another, and keeping that distinction is what
    makes a duplicate fetch visible.
    """
    base = os.path.splitext(os.path.basename(url))[0]
    bird = slug[:-len("sonogram")] if slug.endswith("sonogram") else slug
    ident = base[len(bird):] if base.lower().startswith(bird.lower()) else base
    ident = re.sub(r"[^A-Za-z0-9]+", "", ident) or "0"
    return "fss_%s_%s_%s" % (species.lower(), bird, ident)


def provenance_row(outdir, row):
    path = os.path.join(outdir, "PROVENANCE.tsv")
    new = not os.path.exists(path)
    with open(path, "a", encoding="utf-8") as f:
        if new:
            f.write("# Reference recordings fetched from %s\n" % SITE)
            f.write("# Copyright Fraser Simpson, all rights reserved. "
                    "Not redistributable; measured coefficients only.\n")
            f.write("\t".join(["file", "source", "recording", "copyright",
                               "sha256", "fetched"]) + "\n")
        f.write("\t".join(row) + "\n")


def already(outdir):
    got = set()
    path = os.path.join(outdir, "PROVENANCE.tsv")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            for line in f:
                if not line.startswith("#") and "\t" in line:
                    got.add(line.split("\t")[1])
    return got


def fetch_one(species, slug, url, outdir, done):
    name = stem(species, slug, url)
    mp3 = os.path.join(outdir, name + ".mp3")
    wav = os.path.join(outdir, name + ".wav")
    if url in done and os.path.exists(wav):
        print("      have  %s" % name)
        return False

    blob = get(url, referer=page_url(slug))
    with open(mp3, "wb") as f:
        f.write(blob)
    decode(mp3, wav)

    t = id3_tags(mp3)
    provenance_row(outdir, [
        name + ".wav", url,
        t.get("artist") or t.get("title") or "",
        t.get("comment") or "",
        hashlib.sha256(blob).hexdigest(),
        time.strftime("%Y-%m-%d"),
    ])
    print("      %-46s %6.1f kB  %s"
          % (name, len(blob) / 1024.0, t.get("artist", "")[:60]))
    return True


def main():
    ap = argparse.ArgumentParser(
        description="Fetch bird reference recordings from fssbirding.org.uk.")
    ap.add_argument("species", nargs="*",
                    help="ChirpParade species names (%s)"
                         % ", ".join(SPECIES_ORDER))
    ap.add_argument("--all", action="store_true", help="every species in PAGES")
    ap.add_argument("--page", action="append", default=[], metavar="SLUG",
                    help="one site page by slug, e.g. ravensonogram")
    ap.add_argument("--list", action="store_true",
                    help="show what is available and download nothing")
    ap.add_argument("--outdir", default=OUTDIR)
    ap.add_argument("--delay", type=float, default=DELAY_SEC,
                    help="seconds between requests (default %.1f)" % DELAY_SEC)
    args = ap.parse_args()

    if not args.list and not shutil.which("ffmpeg"):
        print("ffmpeg is needed to decode the mp3s: apt install ffmpeg")
        return 1

    work = []
    for name, slugs in PAGES:
        if args.all or name in args.species:
            work += [(name, s) for s in slugs]
    unknown = [s for s in args.species if s not in SPECIES_ORDER]
    if unknown:
        print("not a ChirpParade species: %s" % ", ".join(unknown))
        print("known: %s" % ", ".join(SPECIES_ORDER))
        return 1
    for slug in args.page:
        work.append(("Whistler", slug))
    if not work:
        ap.print_help()
        return 1

    if not args.list:
        os.makedirs(args.outdir, exist_ok=True)
    done = already(args.outdir) if not args.list else set()

    total = 0
    for species, slug in work:
        try:
            urls = sounds_on(slug)
        except Exception as e:                          # noqa: BLE001
            print("   %-28s failed: %s" % (slug, e))
            continue
        print("   %-28s %-11s %d recording(s)" % (slug, species, len(urls)))
        time.sleep(args.delay)
        for u in urls:
            if args.list:
                print("      %s" % u)
                continue
            try:
                if fetch_one(species, slug, u, args.outdir, done):
                    total += 1
                    time.sleep(args.delay)
            except Exception as e:                      # noqa: BLE001
                print("      failed %s: %s" % (os.path.basename(u), e))

    if args.list:
        return 0

    print("\n%d new recording(s) in %s" % (total, args.outdir))
    if total:
        print("\nThey are beside the library, not in it, so nothing is measured\n"
              "differently yet. To fold them in and refit:\n"
              "   mv %s/*.wav %s/\n"
              "   python3 species.py\n"
              "   .venv/bin/python contours.py --emit"
              % (args.outdir, os.path.dirname(args.outdir.rstrip("/"))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
