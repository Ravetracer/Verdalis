// Audio engine: queue, playback, persistence, Media Session and analyser.
import { store } from './util.js';
import { visualizerAllowed } from './config.js';

const sameRef = (a, b) => !!a && !!b && a.slug === b.slug && a.id === b.id;

function shuffled(arr) {
  const a = arr.slice();
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}

export class Player extends EventTarget {
  constructor(app) {
    super();
    this.app = app;
    this.cfg = app.config.player;
    const a = (this.audio = new Audio());
    a.preload = 'metadata';
    this.queue = [];      // [{slug, id}] – stable refs, survive folder renames
    this.index = -1;
    this.original = null; // queue order before shuffling
    this.shuffle = false;
    this.repeat = 'off';  // off | all | one
    this.quality = store.get('quality', this.cfg.defaultQuality || 'hq');
    this.loading = false;
    this._pendingSeek = null;
    this._lastSave = 0;
    this.ctx = null;
    this.analyser = null;

    a.addEventListener('loadedmetadata', () => {
      if (this._pendingSeek != null) {
        try { a.currentTime = this._pendingSeek; } catch { /* ignore */ }
        this._pendingSeek = null;
      }
      this._position();
      this.emit('time');
    });
    a.addEventListener('play', () => this._state());
    a.addEventListener('pause', () => { this._state(); this.persist(true); });
    a.addEventListener('playing', () => { this.loading = false; this._state(); });
    a.addEventListener('waiting', () => { this.loading = true; this._state(); });
    a.addEventListener('canplay', () => { if (this.loading) { this.loading = false; this._state(); } });
    a.addEventListener('durationchange', () => { this._position(); this.emit('time'); });
    a.addEventListener('progress', () => this.emit('buffer'));
    a.addEventListener('seeked', () => { this._position(); this.emit('time'); });
    a.addEventListener('volumechange', () => {
      store.set('volume', a.volume);
      store.set('muted', a.muted);
      this.emit('volume');
    });
    a.addEventListener('timeupdate', () => {
      this.emit('time');
      if (Date.now() - this._lastSave > 5000) this.persist(true);
    });
    a.addEventListener('ended', () => {
      if (this.cfg.autoAdvance !== false) this.next(true);
      else this._state();
    });
    a.addEventListener('error', () => {
      if (!a.getAttribute('src')) return;
      this.loading = false;
      this._state();
      this.emit('error', { message: `Could not play “${this.current?.track.title ?? 'track'}”.` });
    });

    this._setupMediaSession();
    addEventListener('pagehide', () => this.persist(true));
    document.addEventListener('visibilitychange', () => { if (document.hidden) this.persist(true); });
  }

  emit(type, detail = {}) { this.dispatchEvent(new CustomEvent(type, { detail })); }
  on(type, fn) {
    for (const t of type.split(' ')) this.addEventListener(t, fn);
    return () => type.split(' ').forEach(t => this.removeEventListener(t, fn));
  }

  // ------------------------------------------------------------ getters

  resolve(ref) {
    const album = ref && this.app.bySlug.get(ref.slug);
    const track = album?.trackById.get(ref.id);
    return track ? { album, track } : null;
  }
  get ref() { return this.queue[this.index] || null; }
  get current() { return this.resolve(this.ref); }
  get playing() { return !this.audio.paused && !this.audio.ended; }
  get duration() {
    const d = this.audio.duration;
    return isFinite(d) && d > 0 ? d : this.current?.track.duration || 0;
  }
  get time() { return this._pendingSeek ?? this.audio.currentTime ?? 0; }
  get bufferedFraction() {
    const b = this.audio.buffered, d = this.duration, t = this.audio.currentTime;
    if (!d || !b?.length) return 0;
    for (let i = 0; i < b.length; i++) if (b.start(i) <= t + 0.5 && b.end(i) >= t) return b.end(i) / d;
    return 0;
  }
  isCurrent(slug, id) {
    const r = this.ref;
    return !!r && r.slug === slug && (id == null || r.id === id);
  }

  // ------------------------------------------------------------ queue

  setQueue(refs, index = 0) {
    this.queue = refs.slice();
    this.index = Math.min(Math.max(0, index), refs.length - 1);
    this.original = null;
    if (this.shuffle) this._applyShuffle();
    this.emit('queue');
  }

  playAlbum(slug, startIndex = 0, { shuffle = false, at = 0 } = {}) {
    const album = this.app.bySlug.get(slug);
    if (!album?.tracks.length) return;
    const refs = album.tracks.map(t => ({ slug: album.slug, id: t.id }));
    if (shuffle) {
      this.shuffle = true;
      startIndex = Math.floor(Math.random() * refs.length);
      this.emit('mode');
    }
    this.setQueue(refs, startIndex);
    this.load(this.index, { autoplay: true, time: at });
  }

  enqueue(ref, next = false) {
    if (!this.resolve(ref)) return;
    if (!this.queue.length) { this.setQueue([ref], 0); this.load(0, { autoplay: false }); return; }
    const pos = next ? this.index + 1 : this.queue.length;
    this.queue.splice(pos, 0, ref);
    if (this.original) this.original.push(ref);
    this.emit('queue');
    this.persist();
  }

  removeAt(i) {
    if (i === this.index || i < 0 || i >= this.queue.length) return;
    const [ref] = this.queue.splice(i, 1);
    if (i < this.index) this.index--;
    if (this.original) {
      const k = this.original.findIndex(r => sameRef(r, ref));
      if (k >= 0) this.original.splice(k, 1);
    }
    this.emit('queue');
    this.persist();
  }

  clearUpcoming() {
    this.queue = this.queue.slice(0, this.index + 1);
    this.original = null;
    this.emit('queue');
    this.persist();
  }

  _applyShuffle() {
    const cur = this.ref;
    this.original = this.queue.slice();
    const rest = shuffled(this.queue.filter((_, i) => i !== this.index));
    this.queue = cur ? [cur, ...rest] : rest;
    this.index = cur ? 0 : -1;
  }

  setShuffle(on) {
    if (on === this.shuffle) return;
    this.shuffle = on;
    if (on) this._applyShuffle();
    else if (this.original) {
      const cur = this.ref;
      this.queue = this.original;
      this.index = Math.max(0, this.queue.findIndex(r => sameRef(r, cur)));
      this.original = null;
    }
    this.emit('queue');
    this.emit('mode');
    this.persist();
  }

  cycleRepeat() {
    this.repeat = { off: 'all', all: 'one', one: 'off' }[this.repeat];
    this.emit('mode');
    this.persist();
  }

  // ------------------------------------------------------------ transport

  srcFor(track) {
    return this.quality === 'saver' && track.lossy ? track.lossy : track.src;
  }

  load(index, { autoplay = true, time = 0 } = {}) {
    const cur = this.resolve(this.queue[index]);
    if (!cur) return;
    this.index = index;
    this._pendingSeek = time > 0 ? time : null;
    this.loading = autoplay;
    this.audio.src = this.srcFor(cur.track);
    this._metadata();
    this.emit('track');
    this._state();
    if (autoplay) this.play();
    this.persist();
  }

  async play() {
    if (this.index < 0) { if (this.queue.length) this.load(0); return; }
    if (!this.audio.getAttribute('src')) { this.load(this.index, { autoplay: true, time: this._pendingSeek || 0 }); return; }
    this._ensureGraph();
    try {
      await this.audio.play();
    } catch (e) {
      this.loading = false;
      this._state();
      if (e.name === 'NotSupportedError') this.emit('error', { message: 'This browser cannot play FLAC files.' });
    }
  }

  pause() { this.audio.pause(); }
  toggle() { this.playing ? this.pause() : this.play(); }

  next(auto = false) {
    if (!this.queue.length) return;
    if (auto && this.repeat === 'one') { this.audio.currentTime = 0; this.play(); return; }
    let i = this.index + 1;
    if (i >= this.queue.length) {
      if (this.repeat === 'all') i = 0;
      else if (auto && this.cfg.continueToNextAlbum && this._continueWithNextAlbum()) return;
      else { this.load(0, { autoplay: false }); return; }
    }
    this.load(i, { autoplay: true });
  }

  prev() {
    if (!this.queue.length) return;
    if (this.time > 3 || (this.index === 0 && this.repeat !== 'all')) { this.seek(0); return; }
    this.load(this.index > 0 ? this.index - 1 : this.queue.length - 1, { autoplay: true });
  }

  jumpTo(i) { if (i >= 0 && i < this.queue.length) this.load(i, { autoplay: true }); }

  _continueWithNextAlbum() {
    const list = this.app.albums.filter(a => !a.hidden);
    const i = list.findIndex(a => a.slug === this.ref?.slug);
    const next = list[i + 1] || list[0];
    if (!next || next.slug === this.ref?.slug) return false;
    this.playAlbum(next.slug);
    return true;
  }

  seek(t) {
    const d = this.duration;
    t = Math.max(0, d ? Math.min(t, d - 0.25) : t);
    if (this.audio.readyState >= 1) this.audio.currentTime = t;
    else {
      this._pendingSeek = t;
      if (!this.audio.getAttribute('src') && this.index >= 0) this.load(this.index, { autoplay: false, time: t });
    }
    this.emit('time');
    this._position();
  }
  seekFraction(f) { this.seek(f * this.duration); }

  setVolume(v) { this.audio.volume = Math.max(0, Math.min(1, v)); if (v > 0) this.audio.muted = false; }
  toggleMute() { this.audio.muted = !this.audio.muted; }

  setQuality(q) {
    this.quality = q;
    store.set('quality', q);
    const cur = this.current;
    if (cur?.track.lossy) this.load(this.index, { autoplay: this.playing, time: this.time });
    this.emit('quality');
  }

  // ------------------------------------------------------------ analyser

  _ensureGraph() {
    if (this.ctx) { if (this.ctx.state === 'suspended') this.ctx.resume(); return; }
    if (!visualizerAllowed(this.cfg)) return;
    try {
      const C = window.AudioContext || window.webkitAudioContext;
      this.ctx = new C();
      const src = this.ctx.createMediaElementSource(this.audio);
      this.analyser = this.ctx.createAnalyser();
      this.analyser.fftSize = 2048;
      this.analyser.smoothingTimeConstant = this.cfg.visualizer.smoothing ?? 0.8;
      src.connect(this.analyser);
      this.analyser.connect(this.ctx.destination);
    } catch (e) {
      console.warn('Visualizer disabled:', e);
      this.analyser = null;
    }
  }

  // ------------------------------------------------------------ state & media session

  _state() {
    this.emit('state');
    if ('mediaSession' in navigator) navigator.mediaSession.playbackState = this.playing ? 'playing' : 'paused';
  }

  _setupMediaSession() {
    if (!('mediaSession' in navigator)) return;
    const set = (action, fn) => { try { navigator.mediaSession.setActionHandler(action, fn); } catch { /* unsupported */ } };
    set('play', () => this.play());
    set('pause', () => this.pause());
    set('previoustrack', () => this.prev());
    set('nexttrack', () => this.next());
    set('seekbackward', d => this.seek(this.time - (d.seekOffset || 10)));
    set('seekforward', d => this.seek(this.time + (d.seekOffset || 10)));
    set('seekto', d => this.seek(d.seekTime));
    set('stop', () => { this.pause(); this.seek(0); });
  }

  _metadata() {
    if (!('mediaSession' in navigator) || !window.MediaMetadata) return;
    const c = this.current;
    if (!c) return;
    const abs = u => new URL(u, location.href).href;
    const art = c.album.cover ? [
      { src: abs(c.album.cover.sm), sizes: '480x480', type: 'image/png' },
      { src: abs(c.album.cover.lg), sizes: '1000x1000', type: 'image/png' },
    ] : [];
    navigator.mediaSession.metadata = new MediaMetadata({
      title: c.track.title,
      artist: c.track.artist || c.album.artist || this.app.config.artist,
      album: c.album.title,
      artwork: art,
    });
  }

  _position() {
    if (!navigator.mediaSession?.setPositionState) return;
    const d = this.duration;
    if (!d) return;
    try {
      navigator.mediaSession.setPositionState({ duration: d, playbackRate: this.audio.playbackRate || 1, position: Math.min(this.time, d) });
    } catch { /* ignore */ }
  }

  // ------------------------------------------------------------ persistence

  persist(now = false) {
    if (!this.cfg.rememberPosition) return;
    clearTimeout(this._persistTimer);
    const save = () => {
      this._lastSave = Date.now();
      store.set('player', {
        queue: this.queue, index: this.index, time: Math.round(this.time * 10) / 10,
        shuffle: this.shuffle, repeat: this.repeat, original: this.original,
      });
    };
    if (now) save(); else this._persistTimer = setTimeout(save, 300);
  }

  restore() {
    const vol = Number(store.get('volume', 1));
    this.audio.volume = Number.isFinite(vol) ? Math.max(0, Math.min(1, vol)) : 1;
    this.audio.muted = !!store.get('muted', false);
    // Sync the UI right away instead of relying on the async volumechange event.
    this.emit('volume');
    const s = store.get('player');
    if (!s || !this.cfg.rememberPosition || !Array.isArray(s.queue)) return;
    const cur = s.queue[s.index];
    const queue = s.queue.filter(r => this.resolve(r));
    const index = queue.findIndex(r => sameRef(r, cur));
    if (index < 0) return;
    this.queue = queue;
    this.shuffle = !!s.shuffle;
    this.repeat = ['off', 'all', 'one'].includes(s.repeat) ? s.repeat : 'off';
    this.original = Array.isArray(s.original) ? s.original.filter(r => this.resolve(r)) : null;
    this.emit('queue');
    this.emit('mode');
    this.load(index, { autoplay: false, time: s.time || 0 });
  }
}
