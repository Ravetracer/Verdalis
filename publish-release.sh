#!/usr/bin/env bash
#
# Publishes a suite release built by release.sh as a GitHub release.
#
#   ./release.sh 0.31.0                        build first
#   ./publish-release.sh 0.31.0                tag v0.31.0, push the tag, create
#                                              the release
#   ./publish-release.sh 0.31.0 --draft        the same, as a draft to review
#   ./publish-release.sh 0.31.0 --prerelease   marked as a pre-release
#   ./publish-release.sh 0.31.0 --notes-file notes.md
#                                              these notes instead of the
#                                              generated ones
#
# The release carries the two per-platform archives release.sh writes,
# verdalis-suite-<version>-linux-x86_64.zip and -windows-x86_64.zip, each
# holding every plugin as CLAP and VST3. GitHub adds the source code archives
# for the tag by itself.
#
# The binaries are built locally, not in CI: they need the gitignored CLAP SDK,
# the patched clap-wrapper and the cross-built Windows Cairo. So this script
# checks that the archives were built from exactly the commit it tags, with no
# uncommitted changes, and refuses otherwise.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
version="${1:-}"
shift || true

usage() {
   echo "usage: $0 <version> [--draft] [--prerelease] [--notes-file <file>]" >&2
   exit 1
}
[ -n "$version" ] || usage

draft=0
prerelease=0
notes_file=""
while [ $# -gt 0 ]; do
   case "$1" in
      --draft)      draft=1 ;;
      --prerelease) prerelease=1 ;;
      --notes-file) [ $# -ge 2 ] || usage; notes_file="$2"; shift ;;
      *) echo "unknown option: $1" >&2; usage ;;
   esac
   shift
done

die() { echo "!!  $*" >&2; exit 1; }

tag="v${version}"
base="verdalis-suite-${version}"
dist="${here}/dist"

command -v gh >/dev/null 2>&1 || die "gh (GitHub CLI) not found"
gh auth status >/dev/null 2>&1 || die "gh is not logged in -- run: gh auth login"
command -v unzip >/dev/null 2>&1 || die "unzip not found"

# ------------------------------------------------------------------ the tree
head="$(git -C "$here" rev-parse HEAD)"
if [ -n "$(git -C "$here" status --porcelain --untracked-files=no)" ]; then
   die "uncommitted changes -- commit, rebuild with ./release.sh ${version}, then publish"
fi
git -C "$here" fetch -q origin
if [ -z "$(git -C "$here" branch -r --contains "$head" 2>/dev/null)" ]; then
   die "HEAD ${head:0:7} is not on origin -- push it first"
fi

# ------------------------------------------------------------------ the assets
assets=()
for os in linux windows; do
   zip="${dist}/${base}-${os}-x86_64.zip"
   [ -f "$zip" ] || die "missing ${zip#"$here"/} -- run ./release.sh ${version}"
   built="$(unzip -p "$zip" "${base}-${os}-x86_64/BUILD-INFO.txt" | sed -n 's/^commit //p')"
   [ -n "$built" ] || die "${zip##*/} records no commit -- rebuild with ./release.sh ${version}"
   [ "$built" = "$head" ] || die "${zip##*/} was built from ${built}, HEAD is ${head} -- rebuild"
   assets+=("$zip")
   tgz="${zip%.zip}.tar.gz"
   [ -f "$tgz" ] && assets+=("$tgz")
done

# ------------------------------------------------------------------ the tag
if gh release view "$tag" --repo Ravetracer/Verdalis >/dev/null 2>&1; then
   die "release ${tag} already exists on GitHub -- replace files with: gh release upload ${tag} <files> --clobber"
fi
if git -C "$here" rev-parse -q --verify "refs/tags/${tag}" >/dev/null; then
   tagged="$(git -C "$here" rev-parse "${tag}^{commit}")"
   [ "$tagged" = "$head" ] || die "tag ${tag} already exists and points at ${tagged:0:7}, not HEAD"
else
   git -C "$here" tag -a "$tag" -m "Verdalis Suite ${version}"
fi
git -C "$here" push -q origin "refs/tags/${tag}"

# ------------------------------------------------------------------ the notes
#
# Generated from the archive's own BUILD-INFO.txt and the commit subjects since
# the previous tag. The commit messages here carry the plugin name and version
# in their first line, so the subjects read as a changelog.
notes="$(mktemp)"
trap 'rm -f "$notes"' EXIT
if [ -n "$notes_file" ]; then
   cp "$notes_file" "$notes"
else
   info="$(unzip -p "${assets[0]}" "${base}-linux-x86_64/BUILD-INFO.txt")"
   prev="$(git -C "$here" describe --tags --abbrev=0 --match 'v*' "${tag}^" 2>/dev/null || true)"
   {
      echo "Every Verdalis plugin as CLAP and VST3, in one archive per platform."
      echo "Install instructions are in \`INSTALL.txt\` inside the archive; the PDF manuals are in \`manuals/\`."
      echo
      echo "| Download | Platform |"
      echo "|---|---|"
      echo "| \`${base}-linux-x86_64.zip\` | Linux x86_64 -- CLAP + VST3 |"
      echo "| \`${base}-windows-x86_64.zip\` | Windows x86_64 -- CLAP + VST3 |"
      echo
      echo "## Plugins"
      echo
      echo "| Plugin | Version |"
      echo "|---|---|"
      printf '%s\n' "$info" | awk '/^plugins:/{p=1;next} /^$/{p=0} p{print "| " $1 " | " $2 " |"}'
      echo
      echo "## Changes"
      echo
      if [ -n "$prev" ]; then
         git -C "$here" log --no-merges --format='- %s' "${prev}..${tag}"
         echo
         echo "**Full changelog:** https://github.com/Ravetracer/Verdalis/compare/${prev}...${tag}"
      else
         echo "First release published on GitHub."
      fi
   } > "$notes"
fi

# ------------------------------------------------------------------ publish
args=("$tag" "${assets[@]}" --repo Ravetracer/Verdalis --verify-tag
      --title "Verdalis Suite ${version}" --notes-file "$notes")
[ "$draft" = 1 ] && args+=(--draft)
[ "$prerelease" = 1 ] && args+=(--prerelease)

gh release create "${args[@]}"
