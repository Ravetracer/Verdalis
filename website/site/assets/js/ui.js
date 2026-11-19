// Persistent player UI: bottom bar, full-screen "now playing" view and queue.
import { $, $$, esc, icon, fmtTime, store, toast, share, isTouch } from './util.js';
import { Waveform, loadPeaks } from './waveform.js';
import { waveOptions, playerConfig } from './config.js';
import { Visualizer } from './visualizer.js';

const pp = (cls = '') => `${icon('play', `ic-play ${cls}`)}${icon('pause', `ic-pause ${cls}`)}`;

const TEMPLATE = `
<div class="pbar is-hidden" role="region" aria-label="Player">
  <div class="pbar-wave-m m-only" data-wave="barMobile"></div>
  <div class="pbar-inner">
    <button class="pbar-now" data-open-np aria-label="Open now playing">
      <span class="pbar-cover"><img alt=""></span>
      <span class="pbar-text"><span class="pbar-title"></span><span class="pbar-sub"></span></span>
    </button>
    <div class="pbar-controls">
      <button class="ctl ctl-sm d-only" data-act="shuffle" aria-label="Shuffle" title="Shuffle">${icon('shuffle')}</button>
      <button class="ctl d-only" data-act="prev" aria-label="Previous" title="Previous (P)">${icon('prev')}</button>
      <button class="ctl ctl-play" data-act="toggle" aria-label="Play/Pause" title="Play/Pause (Space)">${pp()}<span class="ctl-spin"></span></button>
      <button class="ctl" data-act="next" aria-label="Next" title="Next (N)">${icon('next')}</button>
      <button class="ctl ctl-sm d-only" data-act="repeat" aria-label="Repeat" title="Repeat">${icon('repeat', 'ic-rep')}${icon('repeat-one', 'ic-rep1')}</button>
    </div>
    <div class="pbar-progress d-only">
      <span class="time t-cur">0:00</span>
      <div class="pbar-wave" data-wave="bar"></div>
      <span class="time t-dur">0:00</span>
    </div>
    <div class="pbar-extra d-only">
      <div class="vol">
        <button class="ctl ctl-sm" data-act="mute" aria-label="Mute" title="Mute (M)">${icon('volume', 'ic-vol')}${icon('mute', 'ic-mute')}</button>
        <input class="vol-range" type="range" min="0" max="1" step="0.01" aria-label="Volume">
      </div>
      <button class="ctl ctl-sm" data-act="queue" aria-label="Queue" title="Queue">${icon('queue')}</button>
      <button class="ctl ctl-sm" data-open-np aria-label="Expand player" title="Expand">${icon('chevron-up')}</button>
    </div>
  </div>
</div>

<div class="np" role="dialog" aria-modal="true" aria-label="Now playing" aria-hidden="true">
  <div class="np-bg"></div>
  <canvas class="np-viz"></canvas>
  <div class="np-inner">
    <header class="np-head">
      <button class="ctl" data-close-np aria-label="Close player">${icon('chevron-down')}</button>
      <div class="np-head-title"><span class="eyebrow">Playing from</span><a class="np-album-link" href="#/"></a></div>
      <button class="ctl" data-act="queue-np" aria-label="Toggle queue" title="Queue">${icon('queue')}</button>
    </header>
    <div class="np-main">
      <div class="np-art"><div class="np-cover"><img alt=""></div></div>
      <div class="np-side">
        <div class="np-info">
          <h2 class="np-title"></h2>
          <p class="np-sub"></p>
        </div>
        <div class="np-wave" data-wave="nowPlaying"></div>
        <div class="np-times"><span class="t-cur">0:00</span><span class="t-dur">0:00</span></div>
        <div class="np-controls">
          <button class="ctl" data-act="shuffle" aria-label="Shuffle">${icon('shuffle')}</button>
          <button class="ctl ctl-lg" data-act="prev" aria-label="Previous">${icon('prev')}</button>
          <button class="ctl ctl-play ctl-xl" data-act="toggle" aria-label="Play/Pause">${pp()}<span class="ctl-spin"></span></button>
          <button class="ctl ctl-lg" data-act="next" aria-label="Next">${icon('next')}</button>
          <button class="ctl" data-act="repeat" aria-label="Repeat">${icon('repeat', 'ic-rep')}${icon('repeat-one', 'ic-rep1')}</button>
        </div>
        <div class="np-extra">
          <div class="vol d-only">
            <button class="ctl ctl-sm" data-act="mute" aria-label="Mute">${icon('volume', 'ic-vol')}${icon('mute', 'ic-mute')}</button>
            <input class="vol-range" type="range" min="0" max="1" step="0.01" aria-label="Volume">
          </div>
          <button class="chip np-quality" data-act="quality" hidden></button>
          <span class="np-format"></span>
          <a class="ctl ctl-sm np-dl" download aria-label="Download track" title="Download FLAC">${icon('download')}</a>
          <button class="ctl ctl-sm" data-act="share" aria-label="Share" title="Share">${icon('share')}</button>
        </div>
      </div>
      <section class="np-queue" aria-label="Queue">
        <div class="np-queue-head"><h3>Up next <span class="q-count"></span></h3><button class="chip" data-act="clear-queue">Clear</button></div>
        <ol class="queue-list"></ol>
      </section>
    </div>
  </div>
</div>`;

export class PlayerUI {
  constructor(app) {
    this.app = app;
    this.p = app.player;
    this.root = document.getElementById('player-root');
    this.root.innerHTML = TEMPLATE;
    this.bar = $('.pbar', this.root);
    this.np = $('.np', this.root);
    this.pageWaves = new Set();
    this.mqDesktop = matchMedia('(min-width: 900px)');
    this._sec = -1;

    const hooks = { onSeek: f => this.p.seekFraction(f), getDuration: () => this.p.duration };
    this.waves = {
      bar: new Waveform($('[data-wave=bar]', this.bar), waveOptions(app.config, 'bar'), hooks),
      barMobile: new Waveform($('[data-wave=barMobile]', this.bar), waveOptions(app.config, 'barMobile'), hooks),
      nowPlaying: new Waveform($('[data-wave=nowPlaying]', this.np), waveOptions(app.config, 'nowPlaying'), hooks),
    };
    this.viz = new Visualizer($('.np-viz', this.np), this.p, () => this._pcfg().visualizer, () => $('.np-cover', this.np));

    new ResizeObserver(() => {
      document.documentElement.style.setProperty('--pbar-h', `${this.bar.offsetHeight}px`);
    }).observe(this.bar);

    this._bindPlayer();
    this._bindControls();
    this._bindKeys();
    this._bindSwipe();
    this._volume();
    this._mode();
  }

  _pcfg() { return playerConfig(this.app.config, this.p.current?.album); }

  // ------------------------------------------------------------ waveform registry

  /** Registers a page waveform (e.g. in a track list) to follow playback. */
  registerWave(wave, slug, id) {
    this.pageWaves.add({ wave, slug, id });
    if (this.p.isCurrent(slug, id)) wave.setProgress(this.p.duration ? this.p.time / this.p.duration : 0, this.p.bufferedFraction);
  }

  _currentWaves() {
    const out = [this.waves.bar, this.waves.barMobile, this.waves.nowPlaying];
    for (const e of this.pageWaves) {
      if (!e.wave.host.isConnected) { e.wave.destroy(); this.pageWaves.delete(e); continue; }
      if (this.p.isCurrent(e.slug, e.id)) out.push(e.wave);
    }
    return out;
  }

  _schedule() {
    if (!this._raf) this._raf = requestAnimationFrame(() => this._tick());
  }

  _tick() {
    this._raf = 0;
    const p = this.p, d = p.duration, t = p.time;
    const f = d ? t / d : 0, b = p.bufferedFraction;
    for (const w of this._currentWaves()) w.setProgress(f, b);
    const sec = Math.floor(t);
    if (sec !== this._sec || this._durDirty) {
      this._sec = sec;
      this._durDirty = false;
      for (const el of $$('.t-cur', this.root)) el.textContent = fmtTime(t);
      for (const el of $$('.t-dur', this.root)) el.textContent = fmtTime(d);
    }
    if (p.playing) this._schedule();
  }

  // ------------------------------------------------------------ player events

  _bindPlayer() {
    const p = this.p;
    p.on('track', () => this._track());
    p.on('state', () => {
      this.root.dataset.playing = String(p.playing);
      this.root.dataset.loading = String(p.loading && !p.audio.paused);
      this.syncMarkers();
      this._viz();
      this._schedule();
    });
    p.on('time buffer', () => this._schedule());
    p.on('queue', () => this.renderQueue());
    p.on('mode', () => this._mode());
    p.on('volume', () => this._volume());
    p.on('quality', () => this._quality());
    p.on('error', e => toast(e.detail.message));
  }

  _track() {
    const c = this.p.current;
    if (!c) return;
    const { album, track } = c;
    const cfg = this.app.config;
    this.bar.classList.remove('is-hidden');
    document.body.classList.add('has-player');

    const accent = cfg.theme.useAlbumAccent && album.accent;
    for (const el of [this.root]) {
      el.style.setProperty('--accent', accent || cfg.theme.accent);
      el.style.setProperty('--accent2', (cfg.theme.useAlbumAccent && album.accent2) || cfg.theme.accent2);
    }

    const sub = `${esc(track.artist || album.artist || '')}${album.artist || track.artist ? ' · ' : ''}${esc(album.title)}`;
    $('.pbar-title', this.bar).textContent = track.title;
    $('.pbar-sub', this.bar).innerHTML = sub;
    $('.np-title', this.np).textContent = track.title;
    $('.np-sub', this.np).innerHTML = `${esc(track.artist || album.artist || '')}`;
    const link = $('.np-album-link', this.np);
    link.href = `#/plugin/${album.slug}`;
    link.textContent = album.title;
    const sm = album.cover?.sm || '', lg = album.cover?.lg || '';
    $('.pbar-cover img', this.bar).src = sm;
    $('.np-cover img', this.np).src = lg;
    this.np.querySelector('.np-bg').style.backgroundImage = lg ? `url("${lg}")` : '';
    const f = track.format;
    $('.np-format', this.np).textContent = f?.bits ? `FLAC ${f.bits}/${+(f.sampleRate / 1000).toFixed(1)}` : '';
    const dl = $('.np-dl', this.np);
    dl.hidden = !cfg.player.trackDownloads;
    dl.href = track.src;

    const pc = this._pcfg();
    this.np.dataset.shape = pc.nowPlaying.coverShape || 'rounded';
    this.np.dataset.spin = String(!!pc.nowPlaying.spin);
    this.np.style.setProperty('--viz-opacity', pc.visualizer.opacity ?? 0.75);

    for (const [ctx, w] of Object.entries(this.waves)) {
      w.setOptions(waveOptions(cfg, ctx, album));
      w.data = null;
      w.setProgress(0, 0);
      w.refreshColors();
    }
    loadPeaks(track.peaks).then(data => {
      if (!this.p.isCurrent(album.slug, track.id)) return;
      for (const w of Object.values(this.waves)) w.setData(data);
    });
    for (const e of this.pageWaves) if (!this.p.isCurrent(e.slug, e.id)) e.wave.setProgress(0, 0);

    this._durDirty = true;
    this._quality();
    this.syncMarkers();
    this.renderQueue();
    this.viz.refreshColors();
    this._schedule();
  }

  syncMarkers() {
    const ref = this.p.ref, playing = this.p.playing;
    for (const el of $$('[data-album]')) {
      const id = el.dataset.trackId;
      const cur = !!ref && el.dataset.album === ref.slug && (!id || id === ref.id);
      el.classList.toggle('is-current', cur);
      el.classList.toggle('is-playing', cur && playing);
    }
  }

  _mode() {
    for (const b of $$('[data-act=shuffle]', this.root)) b.classList.toggle('is-on', this.p.shuffle);
    for (const b of $$('[data-act=repeat]', this.root)) {
      b.classList.toggle('is-on', this.p.repeat !== 'off');
      b.dataset.repeat = this.p.repeat;
      b.title = `Repeat: ${this.p.repeat}`;
    }
  }

  _volume() {
    const a = this.p.audio;
    this.root.dataset.muted = String(a.muted || a.volume === 0);
    // Slider position is perceptual: volume = position², so position = sqrt(volume).
    const pos = a.muted ? 0 : Math.sqrt(a.volume);
    for (const r of $$('.vol-range', this.root)) {
      r.value = pos;
      r.style.setProperty('--val', `${pos * 100}%`);
    }
  }

  _quality() {
    const t = this.p.current?.track;
    for (const b of $$('[data-act=quality]', this.root)) {
      b.hidden = !t?.lossy;
      b.textContent = this.p.quality === 'saver' ? 'Data saver' : 'Lossless';
      b.title = 'Switch between lossless FLAC and a smaller AAC stream';
    }
  }

  _viz() {
    const open = this.np.classList.contains('open');
    if (open && this.p.playing && this.p.analyser && this._pcfg().visualizer.style !== 'none') this.viz.start();
    else this.viz.stop();
  }

  // ------------------------------------------------------------ queue

  renderQueue() {
    const list = $('.queue-list', this.np);
    const p = this.p;
    list.innerHTML = p.queue.map((r, i) => {
      const c = p.resolve(r);
      if (!c) return '';
      const cls = i === p.index ? 'is-now' : i < p.index ? 'is-past' : '';
      return `<li class="q-item ${cls}">
        <button class="q-main" data-q-jump="${i}">
          <img src="${esc(c.album.cover?.sm || '')}" alt="" loading="lazy">
          <span class="q-text"><span class="q-title">${esc(c.track.title)}</span><span class="q-sub">${esc(c.album.title)}</span></span>
          <span class="q-dur">${fmtTime(c.track.duration)}</span>
        </button>
        ${i !== p.index ? `<button class="ctl ctl-xs" data-q-remove="${i}" aria-label="Remove from queue">${icon('close')}</button>` : ''}
      </li>`;
    }).join('');
    $('.q-count', this.np).textContent = p.queue.length ? `· ${Math.max(0, p.queue.length - p.index - 1)}` : '';
    if (this.np.classList.contains('show-queue')) $('.is-now', list)?.scrollIntoView({ block: 'nearest' });
  }

  // ------------------------------------------------------------ now playing overlay

  openNP(showQueue) {
    if (!this.p.current || this.np.classList.contains('open')) return;
    const q = showQueue ?? (this.mqDesktop.matches && store.get('npQueue', false));
    this.np.classList.toggle('show-queue', !!q);
    this.np.classList.add('open');
    this.np.setAttribute('aria-hidden', 'false');
    document.body.classList.add('np-open');
    history.pushState({ ...history.state, np: true }, '');
    requestAnimationFrame(() => {
      this.waves.nowPlaying.resize();
      $('[data-close-np]', this.np).focus({ preventScroll: true });
      if (q) $('.is-now', this.np)?.scrollIntoView({ block: 'nearest' });
    });
    this._viz();
  }

  closeNP(fromHistory = false, then) {
    if (!this.np.classList.contains('open')) { then?.(); return; }
    this.np.classList.remove('open');
    this.np.setAttribute('aria-hidden', 'true');
    this.np.style.transform = '';
    document.body.classList.remove('np-open');
    this.viz.stop();
    if (!fromHistory && history.state?.np) {
      this._afterClose = then;
      history.back();
    } else then?.();
  }

  /** Called by the router on popstate. Returns true if the event was consumed. */
  onPopState() {
    if (this.np.classList.contains('open') && !history.state?.np) {
      this.closeNP(true);
      return true;
    }
    if (this._afterClose) {
      const fn = this._afterClose;
      this._afterClose = null;
      fn();
      return true;
    }
    return false;
  }

  toggleQueue() {
    const on = !this.np.classList.contains('show-queue');
    this.np.classList.toggle('show-queue', on);
    if (this.mqDesktop.matches) store.set('npQueue', on);
    if (on) $('.is-now', this.np)?.scrollIntoView({ block: 'nearest' });
    requestAnimationFrame(() => this.waves.nowPlaying.resize());
  }

  // ------------------------------------------------------------ input

  _bindControls() {
    const p = this.p;
    document.addEventListener('click', e => {
      const t = e.target;
      const act = t.closest('[data-act]');
      if (act && this.root.contains(act)) {
        switch (act.dataset.act) {
          case 'toggle': p.toggle(); break;
          case 'next': p.next(); break;
          case 'prev': p.prev(); break;
          case 'shuffle': p.setShuffle(!p.shuffle); break;
          case 'repeat': p.cycleRepeat(); break;
          case 'mute': p.toggleMute(); break;
          case 'queue': this.openNP(true); break;
          case 'queue-np': this.toggleQueue(); break;
          case 'clear-queue': p.clearUpcoming(); break;
          case 'quality': p.setQuality(p.quality === 'saver' ? 'hq' : 'saver'); break;
          case 'share': {
            const c = p.current;
            if (c) share({ title: `${c.track.title} – ${c.album.title}`, url: `${location.origin}${location.pathname}#/plugin/${c.album.slug}/${c.album.tracks.indexOf(c.track) + 1}` });
            break;
          }
        }
        return;
      }
      if (t.closest('[data-open-np]')) { this.openNP(); return; }
      if (t.closest('[data-close-np]')) { this.closeNP(); return; }
      const albumLink = t.closest('.np-album-link');
      if (albumLink) {
        e.preventDefault();
        e.stopPropagation();
        const href = albumLink.getAttribute('href');
        this.closeNP(false, () => this.app.navigate(href));
        return;
      }
      const jump = t.closest('[data-q-jump]');
      if (jump) { p.jumpTo(+jump.dataset.qJump); return; }
      const rm = t.closest('[data-q-remove]');
      if (rm) { p.removeAt(+rm.dataset.qRemove); return; }

      // Page-level actions (album cards, album pages, track rows)
      const playAlbum = t.closest('[data-play-album]');
      if (playAlbum) {
        e.preventDefault();
        const slug = playAlbum.dataset.playAlbum;
        p.isCurrent(slug) ? p.toggle() : p.playAlbum(slug);
        return;
      }
      const shuffleAlbum = t.closest('[data-shuffle-album]');
      if (shuffleAlbum) { p.playAlbum(shuffleAlbum.dataset.shuffleAlbum, 0, { shuffle: true }); return; }
      const enq = t.closest('[data-enqueue]');
      if (enq) {
        const row = enq.closest('[data-track-id]');
        p.enqueue({ slug: row.dataset.album, id: row.dataset.trackId });
        toast('Added to queue');
        return;
      }
      if (t.closest('a[href], .wf, input, select')) return;
      const row = t.closest('[data-play-track]');
      if (row) {
        const { album, trackId } = row.dataset;
        if (p.isCurrent(album, trackId)) p.toggle();
        else p.playAlbum(album, +row.dataset.playTrack);
      }
    });

    for (const r of $$('.vol-range', this.root)) {
      r.addEventListener('input', () => this.p.setVolume((+r.value) ** 2));
    }
    addEventListener('popstate', () => { /* handled by router via onPopState */ });
  }

  _bindKeys() {
    document.addEventListener('keydown', e => {
      if (e.defaultPrevented || e.ctrlKey || e.metaKey || e.altKey) return;
      const t = e.target;
      if (t.closest?.('input, textarea, select, [contenteditable=""], [contenteditable=true]')) return;
      if (e.key === 'Escape' && this.np.classList.contains('open')) { this.closeNP(); return; }
      if (!this.p.current) return;
      const onControl = t.closest?.('button, a, [role=slider], [role=button]');
      switch (e.key) {
        case ' ':
          if (onControl) return;
          e.preventDefault(); this.p.toggle(); break;
        case 'k': this.p.toggle(); break;
        case 'ArrowRight':
          if (t.closest?.('[role=slider]')) return;
          e.preventDefault(); e.shiftKey ? this.p.next() : this.p.seek(this.p.time + 10); break;
        case 'ArrowLeft':
          if (t.closest?.('[role=slider]')) return;
          e.preventDefault(); e.shiftKey ? this.p.prev() : this.p.seek(this.p.time - 10); break;
        case 'n': case 'N': this.p.next(); break;
        case 'p': case 'P': this.p.prev(); break;
        case 'm': case 'M': this.p.toggleMute(); break;
        case 's': case 'S': this.p.setShuffle(!this.p.shuffle); break;
        case 'r': case 'R': this.p.cycleRepeat(); break;
        default: return;
      }
    });
  }

  /** Swipe down on the cover/header closes the now playing view on touch devices. */
  _bindSwipe() {
    let y0 = null, dy = 0;
    const zone = el => el.closest('.np-head, .np-art');
    this.np.addEventListener('touchstart', e => {
      if (!isTouch() || !zone(e.target) || e.touches.length !== 1) return;
      y0 = e.touches[0].clientY; dy = 0;
      this.np.style.transition = 'none';
    }, { passive: true });
    this.np.addEventListener('touchmove', e => {
      if (y0 == null) return;
      dy = Math.max(0, e.touches[0].clientY - y0);
      this.np.style.transform = `translateY(${dy}px)`;
    }, { passive: true });
    this.np.addEventListener('touchend', () => {
      if (y0 == null) return;
      y0 = null;
      this.np.style.transition = '';
      this.np.style.transform = '';
      if (dy > 110) this.closeNP();
    });
  }
}
