// Resolves download links to the latest GitHub release at page load, so the
// site never has to change when a new version is published.
//
// Verdalis: the newest release of Ravetracer/Verdalis (one suite archive per
// platform). Other plugins: Ravetracer/audio-plugins publishes one release per
// plugin, tagged "<name>-v<version>", so the newest release with that prefix.
//
// The GitHub API allows 60 unauthenticated requests per hour and visitor, so
// answers are cached in localStorage for 30 minutes. When the API cannot be
// reached at all, every button still works: it links to the release page.
import { store } from './util.js';

const API = 'https://api.github.com/repos/';
const TTL = 30 * 60 * 1000;
export const SUITE_REPO = 'Ravetracer/Verdalis';
export const PLUGIN_REPO = 'Ravetracer/audio-plugins';

const inflight = new Map();

function slim(r) {
  return {
    tag: r.tag_name, name: r.name, date: r.published_at, url: r.html_url,
    draft: !!r.draft, pre: !!r.prerelease,
    assets: (r.assets || []).map(a => ({ name: a.name, url: a.browser_download_url, size: a.size })),
  };
}

async function cached(path, map) {
  const key = `gh:${path}`;
  const hit = store.get(key);
  if (hit && Date.now() - hit.t < TTL) return hit.d;
  if (inflight.has(key)) return inflight.get(key);
  const p = (async () => {
    try {
      const r = await fetch(API + path, { headers: { Accept: 'application/vnd.github+json' } });
      if (!r.ok) throw new Error(`GitHub API: HTTP ${r.status}`);
      const d = map(await r.json());
      store.set(key, { t: Date.now(), d });
      return d;
    } catch (e) {
      if (hit) return hit.d; // stale beats nothing
      throw e;
    } finally {
      inflight.delete(key);
    }
  })();
  inflight.set(key, p);
  return p;
}

const cmpVersion = (a, b) => {
  const pa = a.split(/[.-]/).map(n => parseInt(n, 10) || 0), pb = b.split(/[.-]/).map(n => parseInt(n, 10) || 0);
  for (let i = 0; i < Math.max(pa.length, pb.length); i++) if ((pa[i] || 0) !== (pb[i] || 0)) return (pa[i] || 0) - (pb[i] || 0);
  return 0;
};

const find = (assets, re) => assets.find(a => re.test(a.name)) || null;

/** The download set: { version, date, page, linux, windows, mac, web, manual } – each a {url,size,name} or null. */
export async function suiteRelease() {
  const r = await cached(`${SUITE_REPO}/releases/latest`, slim);
  const a = r.assets;
  return {
    version: r.tag.replace(/^v/, ''), date: r.date, page: r.url,
    linux: find(a, /linux.*\.zip$/i),
    windows: find(a, /windows.*\.zip$/i),
    mac: find(a, /macos.*\.zip$/i),
    web: find(a, /\.wclap\.tar\.gz$/i),
    manual: null,
  };
}

export async function pluginRelease({ tag, asset }) {
  const list = await cached(`${PLUGIN_REPO}/releases?per_page=100`, rs => rs.map(slim));
  const mine = list.filter(r => !r.draft && !r.pre && r.tag.startsWith(tag));
  if (!mine.length) throw new Error(`no release tagged ${tag}*`);
  mine.sort((x, y) => cmpVersion(y.tag.slice(tag.length), x.tag.slice(tag.length)));
  const r = mine[0], a = r.assets;
  // Linux and Windows share one archive: <Name>-<version>.zip
  const both = find(a, new RegExp(`^${asset}-[\\d.]+\\.zip$`, 'i'));
  return {
    version: r.tag.slice(tag.length), date: r.date, page: r.url,
    linux: both, windows: both,
    mac: find(a, /macos.*\.zip$/i),
    web: find(a, /\.wclap\.tar\.gz$/i),
    manual: find(a, /manual\.pdf$/i),
  };
}

/** Where a button points before (or without) the API answer. */
export const fallbackPage = release => (release
  ? `https://github.com/${PLUGIN_REPO}/releases?q=${encodeURIComponent(release.tag)}&expanded=true`
  : `https://github.com/${SUITE_REPO}/releases/latest`);
