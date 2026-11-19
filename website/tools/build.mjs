#!/usr/bin/env node
/**
 * Builds the static Verdalis website into ../dist-website (the Verdalis root's
 * dist-website/), ready to upload as it is.
 *
 * Inputs (all in website/):
 *   site/                 the page itself: index.html, assets/, .htaccess
 *   content/plugins.json  every text on the page, per plugin
 *   images/               current window screenshots of every plugin, made by
 *                         tools/capture-verdalis-screenshots.sh and
 *                         tools/capture-ap-screenshots.sh
 *   demos/<slug>/         FLAC demos + manifest.json, made by
 *                         tools/render-verdalis-demos.py and
 *                         tools/render-ap-demos.sh (gitignored, large)
 *
 * Output:
 *   index.html, imprint.html, privacy.html,
 *   .htaccess, site.json, assets/               copied
 *   img/<slug>-<n>.webp                         screenshots, max 1800 px wide
 *   demos/<slug>/*.flac                         the demos
 *   data/catalog.json                           everything the page reads
 *   data/peaks/<md5>.json                       waveform data per demo
 *   data/covers/<slug>.svg|-card.svg|.png       generated cover art
 *
 * Download links are NOT built in: the page asks GitHub for the latest release
 * when it loads (assets/js/releases.js), so a new release needs no rebuild.
 *
 * Usage: node tools/build.mjs [--out=DIR] [--jobs=N]
 * Needs Node 18+ and ffmpeg; google-chrome (optional) rasterises the covers
 * for lock-screen artwork.
 */
import { readFile, writeFile, mkdir, rm, cp, copyFile, open, readdir, stat } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { cpus, tmpdir } from 'node:os';
import { fileURLToPath, pathToFileURL } from 'node:url';

const WEB = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const SUITE = path.dirname(WEB);
const args = Object.fromEntries(process.argv.slice(2).map(a => { const [k, v] = a.replace(/^--/, '').split('='); return [k, v ?? true]; }));
const OUT = path.resolve(args.out || path.join(SUITE, 'dist-website'));
const CACHE = path.join(WEB, '.cache');
const JOBS = Math.max(1, Number(args.jobs) || Math.min(8, cpus().length));
const PEAK_BUCKETS = 2000, PEAK_RATE = 8000;

const { GLYPHS } = await import(pathToFileURL(path.join(WEB, 'site/assets/js/icons.js')));

const log = (...m) => console.log(...m);
const warn = (...m) => console.warn('\x1b[33mwarning:\x1b[0m', ...m);

function run(cmd, argv) {
  return new Promise((resolve, reject) => {
    const p = spawn(cmd, argv, { stdio: ['ignore', 'pipe', 'pipe'] });
    const out = []; let err = '';
    p.stdout.on('data', d => out.push(d));
    p.stderr.on('data', d => { err += d; });
    p.on('error', reject);
    p.on('close', code => (code === 0 ? resolve(Buffer.concat(out)) : reject(new Error(`${cmd} exited with ${code}: ${err.trim().split('\n').slice(-3).join(' ')}`))));
  });
}

async function pool(items, n, fn) {
  let i = 0;
  await Promise.all(Array.from({ length: Math.min(n, items.length) }, async () => { while (i < items.length) { const k = i++; await fn(items[k], k); } }));
}

const which = async bin => { try { await run('which', [bin]); return true; } catch { return false; } };

// ---------------------------------------------------------------- FLAC

/** STREAMINFO (duration, format, audio MD5) of a FLAC file. */
async function readFlac(file) {
  const fh = await open(file, 'r');
  try {
    const { size } = await fh.stat();
    const b = Buffer.alloc(42);
    await fh.read(b, 0, 42, 0);
    if (b.toString('latin1', 0, 4) !== 'fLaC' || (b[4] & 0x7f) !== 0) throw new Error('not a FLAC file');
    const s = b.subarray(8);
    const sampleRate = (s[10] << 12) | (s[11] << 4) | (s[12] >> 4);
    const channels = ((s[12] >> 1) & 7) + 1;
    const bits = (((s[12] & 1) << 4) | (s[13] >> 4)) + 1;
    const samples = (s[13] & 0x0f) * 2 ** 32 + s.readUInt32BE(14);
    let md5 = s.subarray(18, 34).toString('hex');
    if (/^0+$/.test(md5)) md5 = createHash('md5').update(await readFile(file)).digest('hex');
    return { size, sampleRate, channels, bits, duration: samples / sampleRate, md5 };
  } finally { await fh.close(); }
}

/** Peak + RMS per bucket, sqrt-encoded bytes – the format waveform.js reads. */
function computePeaks(file, duration) {
  return new Promise((resolve, reject) => {
    const N = PEAK_BUCKETS, per = Math.max(N, Math.round(duration * PEAK_RATE)) / N;
    const peak = new Float64Array(N), sq = new Float64Array(N), cnt = new Uint32Array(N);
    let frame = 0, rest = null, err = '';
    const p = spawn('ffmpeg', ['-v', 'error', '-nostdin', '-i', file, '-ac', '2', '-ar', String(PEAK_RATE), '-f', 's16le', '-acodec', 'pcm_s16le', 'pipe:1']);
    p.stderr.on('data', d => { err += d; });
    p.stdout.on('data', chunk => {
      if (rest) { chunk = Buffer.concat([rest, chunk]); rest = null; }
      const usable = chunk.length - (chunk.length % 4);
      if (usable < chunk.length) rest = Buffer.from(chunk.subarray(usable));
      const s = new Int16Array(chunk.buffer.slice(chunk.byteOffset, chunk.byteOffset + usable));
      for (let i = 0; i < s.length; i += 2) {
        const l = s[i], r = s[i + 1];
        const a = Math.max(Math.abs(l), Math.abs(r));
        const k = Math.min(N - 1, (frame++ / per) | 0);
        if (a > peak[k]) peak[k] = a;
        sq[k] += (l * l + r * r) * 0.5; cnt[k]++;
      }
    });
    p.on('error', reject);
    p.on('close', code => {
      if (code !== 0) return reject(new Error(`ffmpeg failed: ${err.trim()}`));
      const enc = v => Math.min(255, Math.round(Math.sqrt(Math.min(1, v)) * 255));
      const pk = new Uint8Array(N), rm = new Uint8Array(N);
      for (let i = 0; i < N; i++) { pk[i] = enc(peak[i] / 32768); rm[i] = cnt[i] ? enc(Math.sqrt(sq[i] / cnt[i]) / 32768) : 0; }
      resolve({ v: 1, enc: 'sqrt', buckets: N, duration: Math.round(duration * 1000) / 1000, peak: Buffer.from(pk).toString('base64'), rms: Buffer.from(rm).toString('base64') });
    });
  });
}

// ---------------------------------------------------------------- colour & covers

const hexToRgb = h => [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16));
const rgbToHex = c => `#${c.map(v => Math.round(Math.max(0, Math.min(255, v))).toString(16).padStart(2, '0')).join('')}`;
const mix = (a, b, t) => rgbToHex(hexToRgb(a).map((v, i) => v + (hexToRgb(b)[i] - v) * t));
const escXml = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);

/**
 * Cover art: the plugin's glyph over rings of its accent colour. Square with
 * the wordmark (player, plugin page), or wide without it (home page cards).
 */
function coverSvg(p, { w = 1000, h = 1000, wordmark = true } = {}) {
  const a = p.accent, light = mix(a, '#ffffff', 0.55);
  const bg1 = mix('#06120f', a, 0.16), bg2 = mix('#030807', a, 0.04);
  const cx = w / 2, cy = wordmark ? h * 0.42 : h / 2;
  const size = Math.min(w, h) * (wordmark ? 0.34 : 0.4), s = size / 24;
  const glyphPaths = (GLYPHS[p.icon] || []).map(d => `<path d="${d}"/>`).join('');
  const rings = Array.from({ length: 9 }, (_, i) => {
    const r = size * 0.62 + i * Math.min(w, h) * 0.075;
    return `<circle cx="${cx}" cy="${cy}" r="${r.toFixed(1)}" fill="none" stroke="${a}" stroke-opacity="${(0.22 - i * 0.022).toFixed(3)}" stroke-width="${(2.2 - i * 0.15).toFixed(2)}"/>`;
  }).join('');
  const words = p.wordmark.map((t, i) => `<tspan fill="${i % 2 ? a : '#eef6f1'}">${escXml(t)}</tspan>`).join('');
  const label = p.group === 'verdalis' ? 'VERDALIS' : 'RAVETRACER';
  const fs = w * 0.105 * Math.min(1, 9 / p.name.length);
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}" width="${w}" height="${h}">
<defs>
  <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="${bg1}"/><stop offset="1" stop-color="${bg2}"/></linearGradient>
  <radialGradient id="glow" cx="${cx / w}" cy="${cy / h}" r="${(Math.min(w, h) * 0.5) / Math.max(w, h)}" gradientUnits="objectBoundingBox"><stop offset="0" stop-color="${a}" stop-opacity="0.55"/><stop offset="1" stop-color="${a}" stop-opacity="0"/></radialGradient>
  <filter id="blur" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="${(size * 0.06).toFixed(1)}"/></filter>
</defs>
<rect width="${w}" height="${h}" fill="url(#bg)"/>
<rect width="${w}" height="${h}" fill="url(#glow)"/>
${rings}
<g transform="translate(${cx - 12 * s} ${cy - 12 * s}) scale(${s.toFixed(3)})" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <g stroke="${a}" stroke-width="2.4" filter="url(#blur)" opacity="0.9">${glyphPaths}</g>
  <g stroke="${light}" stroke-width="1.35">${glyphPaths}</g>
</g>
${wordmark ? `<text x="${cx}" y="${h * 0.84}" text-anchor="middle" font-family="Inter, 'Segoe UI', Helvetica, Arial, sans-serif" font-weight="800" font-size="${fs.toFixed(0)}" letter-spacing="${(-fs * 0.03).toFixed(1)}">${words}</text>
<text x="${cx}" y="${h * 0.915}" text-anchor="middle" font-family="Inter, 'Segoe UI', Helvetica, Arial, sans-serif" font-weight="600" font-size="${(w * 0.026).toFixed(0)}" letter-spacing="${(w * 0.009).toFixed(1)}" fill="#eef6f1" fill-opacity="0.5">${label}</text>` : ''}
</svg>
`;
}

/** Rasterises an SVG with headless Chrome (for lock-screen artwork). */
async function svgToPng(chrome, svgFile, pngFile, size) {
  const html = path.join(tmpdir(), `vd-cover-${process.pid}-${path.basename(svgFile)}.html`);
  await writeFile(html, `<!doctype html><style>html,body{margin:0;background:#000}img{display:block;width:${size}px;height:${size}px}</style><img src="${pathToFileURL(svgFile)}">`);
  await run(chrome, ['--headless=new', '--disable-gpu', '--hide-scrollbars', '--no-first-run', '--no-default-browser-check',
    '--allow-file-access-from-files', `--window-size=${size},${size}`, `--screenshot=${pngFile}`, pathToFileURL(html).href]);
  await rm(html, { force: true });
}

// ---------------------------------------------------------------- main

async function main() {
  const t0 = Date.now();
  const content = JSON.parse(await readFile(path.join(WEB, 'content/plugins.json'), 'utf8'));
  for (const bin of ['ffmpeg', 'ffprobe']) if (!(await which(bin))) throw new Error(`${bin} is required`);
  const chrome = (await which('google-chrome')) ? 'google-chrome' : (await which('chromium')) ? 'chromium' : null;

  log(`Building into ${OUT}`);
  await rm(OUT, { recursive: true, force: true });
  await mkdir(OUT, { recursive: true });
  await cp(path.join(WEB, 'site'), OUT, { recursive: true });
  for (const d of ['img', 'demos', 'data/peaks', 'data/covers']) await mkdir(path.join(OUT, d), { recursive: true });
  await mkdir(path.join(CACHE, 'peaks'), { recursive: true });

  // Brand images: favicon/touch icon and the social preview.
  const designs = path.join(SUITE, '_designs');
  await run('ffmpeg', ['-v', 'error', '-y', '-i', path.join(designs, 'verdalis-emblem-2048.png'), '-vf', 'scale=180:180:flags=lanczos', path.join(OUT, 'assets/img/apple-touch-icon.png')]);
  await run('ffmpeg', ['-v', 'error', '-y', '-f', 'lavfi', '-i', 'color=c=0x06120f:s=1200x630', '-i', path.join(designs, 'verdalis-logo-horizontal-dark-4000.png'),
    '-filter_complex', '[1:v]scale=980:-1:flags=lanczos[l];[0:v][l]overlay=(W-w)/2:(H-h)/2', '-frames:v', '1', path.join(OUT, 'assets/img/og-image.png')]);

  const plugins = [];
  const peakJobs = [];
  for (const p of content.plugins) {
    const accent2 = p.accent2 || mix(p.accent, '#ffffff', 0.38);

    // Covers
    const sq = path.join(OUT, 'data/covers', `${p.slug}.svg`);
    await writeFile(sq, coverSvg(p));
    await writeFile(path.join(OUT, 'data/covers', `${p.slug}-card.svg`), coverSvg(p, { w: 800, h: 500, wordmark: false }));
    const cover = { svg: `data/covers/${p.slug}.svg`, card: `data/covers/${p.slug}-card.svg`, sm: `data/covers/${p.slug}.svg`, lg: `data/covers/${p.slug}.svg` };
    if (chrome) {
      try {
        await svgToPng(chrome, sq, path.join(OUT, 'data/covers', `${p.slug}.png`), 512);
        cover.sm = cover.lg = `data/covers/${p.slug}.png`;
      } catch (e) { warn(`${p.slug}: cover PNG failed (${e.message}); using the SVG`); }
    }

    // Screenshots -> WebP
    const images = [];
    for (const [i, im] of (p.images || []).entries()) {
      const src = path.resolve(WEB, im.file);
      if (!existsSync(src)) { warn(`${p.slug}: missing image ${im.file}`); continue; }
      const name = `${p.slug}-${i + 1}.webp`;
      const dst = path.join(OUT, 'img', name);
      await run('ffmpeg', ['-v', 'error', '-y', '-i', src, '-vf', "scale='min(1800,iw)':-2:flags=lanczos", '-c:v', 'libwebp', '-quality', '88', '-compression_level', '6', dst]);
      const [w, h] = (await run('ffprobe', ['-v', 'error', '-select_streams', 'v', '-show_entries', 'stream=width,height', '-of', 'csv=p=0', dst])).toString().trim().split(',').map(Number);
      images.push({ src: `img/${name}`, alt: im.alt, w, h });
    }

    // Demos
    const tracks = [];
    const dir = path.join(WEB, 'demos', p.slug);
    const manifestFile = path.join(dir, 'manifest.json');
    const manifest = existsSync(manifestFile) ? JSON.parse(await readFile(manifestFile, 'utf8')) : [];
    if (!manifest.length) warn(`${p.slug}: no demos (${path.relative(SUITE, manifestFile)} missing or empty)`);
    const entries = manifest.slice(0, 10).map(m => ({ ...m, abs: path.join(dir, m.file) }));
    if (p.spotlight) entries.push({ ...p.spotlight, abs: path.join(WEB, 'demos', p.spotlight.file), spotlight: true });
    for (const [i, m] of entries.entries()) {
      if (!existsSync(m.abs)) { warn(`${p.slug}: missing ${m.file}`); continue; }
      const info = await readFlac(m.abs);
      const fname = path.basename(m.abs);
      await mkdir(path.join(OUT, 'demos', p.slug), { recursive: true });
      await copyFile(m.abs, path.join(OUT, 'demos', p.slug, fname));
      const description = p.demoText?.[m.file] ?? m.description ?? '';
      if (!description && !m.spotlight) warn(`${p.slug}: no description for ${m.file}`);
      const cached = path.join(CACHE, 'peaks', `${info.md5}.json`);
      if (!existsSync(cached)) peakJobs.push({ file: m.abs, duration: info.duration, cached, label: `${p.name} – ${m.title}` });
      tracks.push({
        id: info.md5, n: i + 1, title: m.title, description,
        ...(m.artist ? { artist: m.artist } : {}), ...(m.spotlight ? { spotlight: true } : {}),
        duration: Math.round(info.duration * 1000) / 1000, size: info.size,
        format: { sampleRate: info.sampleRate, bits: info.bits, channels: info.channels },
        src: `demos/${p.slug}/${encodeURIComponent(fname)}`,
        peaks: `data/peaks/${info.md5}.json`,
      });
    }

    const { demoText, spotlight, images: _sources, ...rest } = p;
    plugins.push({
      ...rest, accent2, title: p.name, artist: p.group === 'verdalis' ? 'Verdalis' : 'Ravetracer',
      cover, images, tracks, duration: Math.round(tracks.reduce((s, t) => s + t.duration, 0)),
    });
  }

  if (peakJobs.length) log(`Computing ${peakJobs.length} waveform(s)…`);
  let done = 0;
  await pool(peakJobs, JOBS, async j => {
    await writeFile(j.cached, JSON.stringify(await computePeaks(j.file, j.duration)));
    log(`  [${++done}/${peakJobs.length}] ${j.label}`);
  });
  for (const p of plugins) for (const t of p.tracks) await copyFile(path.join(CACHE, 'peaks', `${t.id}.json`), path.join(OUT, t.peaks));

  await writeFile(path.join(OUT, 'data/catalog.json'), JSON.stringify({ version: 1, generated: new Date().toISOString(), groups: content.groups, plugins }));

  // Redirects from the old WordPress permalinks.
  const ht = path.join(OUT, '.htaccess');
  const rules = plugins.flatMap(p => (p.aliases || []).map(a => `  RewriteRule ^(uncategorized|non-verdalis)/${a}/?$ /#/plugin/${p.slug} [R=301,NE,L]`));
  await writeFile(ht, (await readFile(ht, 'utf8')).replace('  # @PLUGIN_REDIRECTS@', rules.join('\n')));

  const count = plugins.reduce((s, p) => s + p.tracks.length, 0);
  const bytes = (await du(OUT)) / 1048576;
  log(`Done: ${plugins.length} plugins, ${count} demos, ${bytes.toFixed(0)} MB in ${((Date.now() - t0) / 1000).toFixed(1)} s → ${OUT}`);
  if (!chrome) warn('no google-chrome/chromium found: covers are SVG only (lock-screen artwork may be missing)');
}

async function du(dir) {
  let total = 0;
  for (const e of await readdir(dir, { withFileTypes: true })) {
    const f = path.join(dir, e.name);
    total += e.isDirectory() ? await du(f) : (await stat(f)).size;
  }
  return total;
}

main().catch(e => { console.error('\x1b[31merror:\x1b[0m', e.message); process.exit(1); });
