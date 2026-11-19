// Page views: home (both plugin groups and the suite download) and the
// per-plugin page. Plugins are "albums" to the player and their demos "tracks".
import { $, $$, esc, icon, fmtTime, fmtBytes, fmtDate } from './util.js';
import { Waveform, loadPeaks } from './waveform.js';
import { waveOptions } from './config.js';
import { glyph } from './icons.js';
import { suiteRelease, pluginRelease, fallbackPage, SUITE_REPO, PLUGIN_REPO } from './releases.js';

const ppIcons = `${icon('play', 'ic-play')}${icon('pause', 'ic-pause')}`;
const accentStyle = p => `--accent:${esc(p.accent)};--accent2:${esc(p.accent2 || p.accent)}`;

const wordmark = p => p.wordmark.map((w, i) => `<span class="wm-${i % 2 ? 'b' : 'a'}">${esc(w)}</span>`).join('');

const presetLine = p => (p.presets ? `${p.presets} ${p.presetNoun || 'presets'}` : '');

// ---------------------------------------------------------------- downloads

const PLATFORMS = [
  { key: 'linux', label: 'Linux', icon: 'linux', sub: 'CLAP · VST3' },
  { key: 'windows', label: 'Windows', icon: 'windows', sub: 'CLAP · VST3' },
  { key: 'mac', label: 'macOS', icon: 'mac', sub: 'CLAP · VST3 · AU' },
  { key: 'web', label: 'WebCLAP', icon: 'web', sub: 'for browser DAWs' },
];

/**
 * Download block. Renders immediately with every button pointing at the
 * release page, then swaps in the direct file links once GitHub answers.
 * The WebCLAP button only appears when the release actually has one.
 */
function downloadBlock(release, { title, note } = {}) {
  const fallback = fallbackPage(release);
  return `<div class="dlbox" data-release="${esc(release ? JSON.stringify(release) : 'suite')}">
    <div class="dlbox-head">
      <div>
        <h3 class="dlbox-title">${esc(title || 'Download')}</h3>
        <p class="dlbox-ver"><span class="dl-version">Latest release</span></p>
      </div>
      <a class="dlbox-gh" href="${esc(fallback)}" target="_blank" rel="noopener">${glyph('github')}<span>All files on GitHub</span></a>
    </div>
    <div class="platforms">
      ${PLATFORMS.map(pl => `<a class="platform" data-platform="${pl.key}" href="${esc(fallback)}" target="_blank" rel="noopener"${pl.key === 'web' ? ' hidden' : ''}>
        <span class="platform-ic">${glyph(pl.icon)}</span>
        <span class="platform-text"><span class="platform-name">${pl.label}</span><span class="platform-sub">${pl.sub}</span></span>
        <span class="platform-size"></span>
        ${icon('download', 'platform-dl')}
      </a>`).join('')}
    </div>
    ${note ? `<p class="dlbox-note">${note}</p>` : ''}
    <details class="install">
      <summary>How to install</summary>
      <div class="install-grid">
        <div><h4>Linux</h4><p>Copy the <code>&lt;Name&gt;</code> folders into <code>~/.clap/</code> and the <code>.vst3</code> folders into <code>~/.vst3/</code>, then let your DAW rescan.</p></div>
        <div><h4>Windows</h4><p>Copy the <code>&lt;Name&gt;</code> folders into <code>C:\\Program Files\\Common Files\\CLAP\\</code> and the <code>.vst3</code> folders into <code>…\\Common Files\\VST3\\</code>.</p></div>
        <div><h4>macOS</h4><p>Copy into <code>~/Library/Audio/Plug-Ins/</code> (CLAP, VST3, Components). The plugins are free and not notarized by Apple, so clear the quarantine once in Terminal:</p>
          <pre><code>xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/CLAP
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components</code></pre></div>
        <div class="install-web" hidden><h4>WebCLAP</h4><p>For DAWs that run in the browser, such as <a href="https://opendaw.studio" target="_blank" rel="noopener">openDAW</a>: import the <code>.wclap.tar.gz</code> file as a plugin. No installation on your computer at all.</p></div>
      </div>
      <p class="install-foot">Every archive also contains an <code>INSTALL.txt</code> with the details.</p>
    </details>
  </div>`;
}

function fillDownloads(root) {
  for (const box of $$('.dlbox', root)) {
    const raw = box.dataset.release;
    const rel = raw === 'suite' ? null : JSON.parse(raw);
    (rel ? pluginRelease(rel) : suiteRelease()).then(d => {
      if (!box.isConnected) return;
      $('.dl-version', box).textContent = `Version ${d.version}${d.date ? ` · ${fmtDate(d.date)}` : ''}`;
      $('.dlbox-gh', box).href = d.page;
      for (const a of $$('[data-platform]', box)) {
        const f = d[a.dataset.platform];
        if (f) {
          a.href = f.url;
          a.removeAttribute('target');
          a.setAttribute('download', '');
          a.title = f.name;
          $('.platform-size', a).textContent = fmtBytes(f.size);
          a.hidden = false;
          a.classList.remove('is-missing');
        } else if (a.dataset.platform === 'web') {
          a.hidden = true;
        } else {
          a.href = d.page;
          a.classList.add('is-missing');
          $('.platform-size', a).textContent = 'see GitHub';
        }
      }
      $('.install-web', box).hidden = !d.web;
      if (d.manual && !$('.dl-manual', box)) {
        $('.dlbox-head > div', box).insertAdjacentHTML('beforeend',
          `<a class="dl-manual" href="${esc(d.manual.url)}" target="_blank" rel="noopener">${glyph('book')}<span>Manual (PDF)</span></a>`);
      }
    }).catch(e => {
      console.warn('Release lookup failed, using release page links:', e);
      if (box.isConnected) $('.dl-version', box).textContent = 'Latest release on GitHub';
    });
  }
}

// ---------------------------------------------------------------- cards

function cardHtml(p) {
  const first = p.tracks.length ? `<button class="card-play pp" data-play-album="${esc(p.slug)}" aria-label="Play ${esc(p.name)} demos">${ppIcons}</button>` : '';
  return `<article class="pcard" data-album="${esc(p.slug)}" style="${accentStyle(p)}">
    <a class="pcard-link" href="#/plugin/${esc(p.slug)}" aria-label="${esc(p.name)}"></a>
    <div class="pcard-art">
      <img src="${esc(p.cover.card)}" alt="" loading="lazy" decoding="async">
      ${first}
    </div>
    <div class="pcard-body">
      <h3 class="pcard-title">${wordmark(p)}</h3>
      <p class="pcard-text">${esc(p.card)}</p>
      <p class="pcard-meta">${[presetLine(p), p.tracks.length ? `${p.tracks.filter(t => !t.spotlight).length} demos` : ''].filter(Boolean).map(esc).join(' · ')}</p>
    </div>
  </article>`;
}

// ---------------------------------------------------------------- home

export function renderHome(app, root, section) {
  const { groups } = app.catalog;
  const by = g => app.albums.filter(p => p.group === g);
  const verdalis = by('verdalis'), other = by('other');
  const totalPresets = verdalis.reduce((s, p) => s + (p.presets || 0), 0);
  const scene = app.bySlug.get('verdaliscene');

  root.innerHTML = `
  <section class="home-hero">
    <div class="hero-glow" aria-hidden="true"></div>
    <img class="hero-emblem" src="assets/img/verdalis-emblem.svg" alt="">
    <h1 class="hero-wordmark">Verdalis</h1>
    <p class="hero-tag">Natural sound synthesis</p>
    <p class="hero-lead">Free audio plugins that <em>synthesise</em> nature instead of sampling it.</p>
    <p class="hero-sub">Rain, thunder, surf, wind, birds, rivers, fire, insects and the night — computed while they play, so nothing ever loops.</p>
    <div class="actions hero-actions">
      <a class="btn btn-primary" href="#/download">${icon('download')}<span>Download the suite</span></a>
      ${scene?.tracks.length ? `<button class="btn pp" data-play-album="verdaliscene">${ppIcons}<span class="lbl-play">Listen to a scene</span><span class="lbl-pause">Pause</span></button>` : ''}
    </div>
    <ul class="stats">
      <li><strong>${verdalis.length}</strong><span>instruments</span></li>
      <li><strong>${totalPresets}</strong><span>presets</span></li>
      <li><strong>0</strong><span>samples</span></li>
      <li><strong>100%</strong><span>free · MIT</span></li>
    </ul>
  </section>

  <section class="group" id="suite">
    <div class="group-head">
      <p class="eyebrow">${esc(groups.verdalis.eyebrow)}</p>
      <h2 class="group-title">${esc(groups.verdalis.title)}</h2>
      <p class="group-text">${esc(groups.verdalis.text)}</p>
    </div>
    <div class="pgrid">${verdalis.map(cardHtml).join('')}</div>
  </section>

  <section class="group group-dl" id="download">
    <div class="group-head">
      <p class="eyebrow">One download, everything inside</p>
      <h2 class="group-title">Get the Verdalis suite</h2>
      <p class="group-text">All ${verdalis.length} instruments in one archive, as CLAP and VST3 — plus Audio Units on macOS. Free and MIT licensed, always the latest version.</p>
    </div>
    ${downloadBlock(null, { title: 'Verdalis suite' })}
  </section>

  <section class="group" id="beyond">
    <div class="group-head">
      <p class="eyebrow">${esc(groups.other.eyebrow)}</p>
      <h2 class="group-title">${esc(groups.other.title)}</h2>
      <p class="group-text">${esc(groups.other.text)}</p>
    </div>
    <div class="pgrid">${other.map(cardHtml).join('')}</div>
  </section>

  <section class="group about-sec" id="about">
    <div class="about-grid">
      <div class="about-card">
        <span class="about-ic">${glyph('sliders')}</span>
        <h3>How these were made</h3>
        <p>These plugins were vibe coded: written with AI (Claude) doing most of the typing, from direction, reference recordings and listening notes. Every model is fitted against real recordings, checked by ear, and every build runs a self-test.</p>
        <p>It's free software, given as-is and tested by one person in Bitwig Studio and Reaper. If something misbehaves in your host, please say so.</p>
      </div>
      <div class="about-card">
        <span class="about-ic">${glyph('github')}</span>
        <h3>Open source</h3>
        <p>Everything is on GitHub — the code, the measurements and what each model was fitted to.</p>
        <div class="links">
          <a class="link-pill" href="https://github.com/${SUITE_REPO}" target="_blank" rel="noopener">${glyph('github')} Verdalis</a>
          <a class="link-pill" href="https://github.com/${PLUGIN_REPO}" target="_blank" rel="noopener">${glyph('github')} Other plugins</a>
        </div>
      </div>
      <div class="about-card">
        <span class="about-ic">${glyph('message')}</span>
        <h3>Found a bug?</h3>
        <p>Bug reports and ideas are welcome in the issue trackers.</p>
        <div class="links">
          <a class="link-pill" href="https://github.com/${SUITE_REPO}/issues" target="_blank" rel="noopener">Verdalis issues</a>
          <a class="link-pill" href="https://github.com/${PLUGIN_REPO}/issues" target="_blank" rel="noopener">Other plugins' issues</a>
        </div>
      </div>
    </div>
  </section>`;

  fillDownloads(root);
  if (section) requestAnimationFrame(() => $(`#${section}`, root)?.scrollIntoView({ block: 'start' }));
  return { title: 'Verdalis – synthesised nature' };
}

// ---------------------------------------------------------------- plugin page

function trackRow(p, t, i) {
  return `<div class="track" data-album="${esc(p.slug)}" data-track-id="${esc(t.id)}" data-play-track="${i}">
    <div class="t-num">
      <span class="n">${String(t.n).padStart(2, '0')}</span>
      <span class="eq" aria-hidden="true"><i></i><i></i><i></i></span>
      ${icon('play', 't-ic t-ic-play')}${icon('pause', 't-ic t-ic-pause')}
    </div>
    <div class="t-title">
      <button class="t-main" aria-label="Play ${esc(t.title)}"><span class="t-name">${esc(t.title)}</span></button>
      ${t.description ? `<span class="t-desc">${esc(t.description)}</span>` : ''}
    </div>
    <div class="t-wave"></div>
    <div class="t-dur">${fmtTime(t.duration)}</div>
  </div>`;
}

export function renderPlugin(app, root, p, focusTrack) {
  const group = app.albums.filter(x => x.group === p.group);
  const idx = group.indexOf(p);
  const prev = group[(idx - 1 + group.length) % group.length];
  const next = group[(idx + 1) % group.length];
  const demos = p.tracks.filter(t => !t.spotlight);
  const spot = p.tracks.find(t => t.spotlight);
  const isSuite = p.group === 'verdalis';
  const formats = isSuite ? ['CLAP', 'VST3', 'AU'] : ['CLAP', 'VST3', 'AU', ...(p.release ? ['WebCLAP'] : [])];
  const chips = [presetLine(p), ...formats, 'Linux', 'Windows', 'macOS', p.license === 'GPL' ? 'GPL' : 'MIT'].filter(Boolean);
  const hero = p.images[0];

  root.innerHTML = `
  <article class="plugin" data-album="${esc(p.slug)}" style="${accentStyle(p)}">
    <div class="plugin-backdrop" style="background-image:url('${esc(hero?.src || p.cover.svg)}')"></div>

    <header class="plugin-hero">
      <div class="plugin-info">
        <a class="crumb" href="#/${isSuite ? 'suite' : 'beyond'}">${glyph('arrowRight', 'flip')} ${isSuite ? 'Verdalis suite' : 'Beyond nature'}</a>
        <h1 class="plugin-title">${wordmark(p)}</h1>
        <p class="plugin-tagline">${esc(p.tagline)}</p>
        <p class="plugin-intro">${esc(p.intro)}</p>
        <ul class="chips-static">${chips.map(c => `<li>${esc(c)}</li>`).join('')}</ul>
        <div class="actions">
          ${demos.length ? `<button class="btn btn-primary pp" data-play-album="${esc(p.slug)}">${ppIcons}<span class="lbl-play">Play the demos</span><span class="lbl-pause">Pause</span></button>` : ''}
          <a class="btn" href="#/plugin/${esc(p.slug)}" data-scroll="get">${icon('download')}<span>Download</span></a>
          <button class="btn btn-icon" data-share aria-label="Share" title="Share">${icon('share')}</button>
        </div>
      </div>
      <div class="plugin-cover">
        <img src="${esc(p.cover.svg)}" alt="">
      </div>
    </header>

    ${hero ? `<figure class="window-shot" data-lightbox="0">
      <img src="${esc(hero.src)}" width="${hero.w}" height="${hero.h}" alt="${esc(hero.alt)}" decoding="async">
      <span class="shot-zoom">${glyph('maximize')}</span>
    </figure>` : ''}

    <section class="highlights">
      ${p.highlights.map((h, i) => `<div class="hl"><span class="hl-n">${String(i + 1).padStart(2, '0')}</span><h3>${esc(h.title)}</h3><p>${esc(h.text)}</p></div>`).join('')}
    </section>

    ${demos.length ? `<section class="listen">
      <div class="section-head"><h2>${glyph(p.icon)} Listen</h2><span class="section-note">${demos.length} demos, straight out of the plugin · FLAC</span></div>
      <div class="tracks layout-waveform">${p.tracks.map((t, i) => (t.spotlight ? '' : trackRow(p, t, i))).join('')}</div>
    </section>` : ''}

    ${spot ? `<section class="spotlight" data-album="${esc(p.slug)}" data-track-id="${esc(spot.id)}" data-play-track="${p.tracks.indexOf(spot)}">
      <button class="spot-play pp" aria-label="Play ${esc(spot.title)}">${ppIcons}</button>
      <div class="spot-text">
        <p class="eyebrow">Made with ${esc(p.name)}</p>
        <h3>“${esc(spot.title)}” by ${esc(spot.artist)}</h3>
        <p>${esc(spot.description)}</p>
      </div>
      <div class="t-wave spot-wave"></div>
    </section>` : ''}

    ${p.images.length > 1 ? `<section class="gallery">
      <div class="section-head"><h2>A closer look</h2></div>
      <div class="gallery-grid">${p.images.slice(1).map((im, i) => `<figure data-lightbox="${i + 1}"><img src="${esc(im.src)}" width="${im.w}" height="${im.h}" alt="${esc(im.alt)}" loading="lazy" decoding="async"></figure>`).join('')}</div>
    </section>` : ''}

    <section class="get" id="get">
      ${isSuite
        ? downloadBlock(null, { title: `${p.name} is part of the Verdalis suite`, note: `One download installs all eleven Verdalis instruments, ${esc(p.name)} included.` })
        : downloadBlock(p.release, { title: `Download ${p.name}`, note: 'Linux and Windows come in one archive together.' })}
      ${p.source ? `<p class="source-link"><a href="${esc(p.source)}" target="_blank" rel="noopener">${glyph('github')} Source code on GitHub</a></p>` : ''}
      ${p.notice ? `<p class="notice">${esc(p.notice)}</p>` : ''}
    </section>

    <nav class="album-nav" aria-label="More plugins">
      <a class="album-nav-link" href="#/plugin/${esc(prev.slug)}" style="${accentStyle(prev)}">${icon('chevron-left')}<span class="anl-cover"><img src="${esc(prev.cover.svg)}" alt="" loading="lazy"></span><span><span class="eyebrow">Previous</span><span class="anl-title">${esc(prev.name)}</span></span></a>
      <a class="album-nav-link next" href="#/plugin/${esc(next.slug)}" style="${accentStyle(next)}"><span><span class="eyebrow">Next</span><span class="anl-title">${esc(next.name)}</span></span><span class="anl-cover"><img src="${esc(next.cover.svg)}" alt="" loading="lazy"></span>${icon('chevron-right')}</a>
    </nav>
  </article>`;

  // Waveforms for every demo row (and the spotlight).
  for (const row of $$('[data-track-id]', root)) {
    const t = p.trackById.get(row.dataset.trackId);
    const i = +row.dataset.playTrack;
    const host = $('.t-wave', row);
    const wave = new Waveform(host, waveOptions(app.config, 'trackList', p), {
      label: `Seek in ${t.title}`,
      getDuration: () => t.duration,
      onSeek: f => {
        if (app.player.isCurrent(p.slug, t.id)) app.player.seekFraction(f);
        else app.player.playAlbum(p.slug, i, { at: f * t.duration });
      },
    });
    app.ui.registerWave(wave, p.slug, t.id);
    loadPeaks(t.peaks).then(d => wave.setData(d));
  }

  root.querySelector('[data-scroll]')?.addEventListener('click', e => {
    e.preventDefault();
    e.stopPropagation();
    $('#get', root)?.scrollIntoView({ behavior: 'smooth', block: 'start' });
  });
  root.querySelector('[data-share]')?.addEventListener('click', () => app.share(p));
  for (const fig of $$('[data-lightbox]', root)) fig.addEventListener('click', () => lightbox(p.images, +fig.dataset.lightbox));

  fillDownloads(root);

  if (focusTrack) {
    const row = $$('.listen .track', root)[focusTrack - 1];
    if (row) {
      row.classList.add('is-target');
      requestAnimationFrame(() => row.scrollIntoView({ block: 'center' }));
    }
  }
  return { title: `${p.name} – ${p.tagline.replace(/\.$/, '')} · Verdalis` };
}

// ---------------------------------------------------------------- lightbox

function lightbox(images, start) {
  let i = start;
  const dlg = document.createElement('dialog');
  dlg.className = 'lightbox';
  const show = () => {
    const im = images[i];
    dlg.innerHTML = `<figure><img src="${esc(im.src)}" alt="${esc(im.alt)}"><figcaption>${esc(im.alt)}</figcaption></figure>
      <button class="ctl lb-close" aria-label="Close">${icon('close')}</button>
      ${images.length > 1 ? `<button class="ctl lb-prev" aria-label="Previous">${icon('chevron-left')}</button><button class="ctl lb-next" aria-label="Next">${icon('chevron-right')}</button>` : ''}`;
  };
  show();
  dlg.addEventListener('click', e => {
    if (e.target.closest('.lb-prev')) { i = (i - 1 + images.length) % images.length; show(); return; }
    if (e.target.closest('.lb-next')) { i = (i + 1) % images.length; show(); return; }
    if (e.target.closest('.lb-close') || e.target === dlg) dlg.close();
  });
  dlg.addEventListener('keydown', e => {
    if (e.key === 'ArrowLeft' && images.length > 1) { i = (i - 1 + images.length) % images.length; show(); }
    if (e.key === 'ArrowRight' && images.length > 1) { i = (i + 1) % images.length; show(); }
  });
  dlg.addEventListener('close', () => dlg.remove());
  document.body.appendChild(dlg);
  dlg.showModal();
}

export function renderNotFound(app, root) {
  root.innerHTML = `<section class="empty"><h1 class="page-title">Nothing grows here</h1><p>This page does not exist (anymore).</p><a class="btn btn-primary" href="#/">Back to all plugins</a></section>`;
  return { title: 'Not found · Verdalis' };
}
