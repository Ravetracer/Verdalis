// Live audio visualizer (needs the Web Audio analyser from the player).
export const VIZ_STYLES = {
  radial: 'Radial ring around the cover',
  spectrum: 'Spectrum bars',
  mirror: 'Mirrored spectrum',
  scope: 'Oscilloscope',
  none: 'Off',
};

export class Visualizer {
  /**
   * @param {HTMLCanvasElement} canvas
   * @param {import('./player.js').Player} player
   * @param {() => object} getOpts    visualizer config ({style, bars, opacity})
   * @param {() => HTMLElement} getAnchor  element to wrap the radial style around
   */
  constructor(canvas, player, getOpts, getAnchor) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.player = player;
    this.getOpts = getOpts;
    this.getAnchor = getAnchor;
    this.raf = 0;
    this.smooth = null;
  }

  start() {
    if (this.raf || !this.player.analyser) return;
    this.refreshColors();
    const loop = () => { this.raf = requestAnimationFrame(loop); this.frame(); };
    this.raf = requestAnimationFrame(loop);
  }

  stop() {
    cancelAnimationFrame(this.raf);
    this.raf = 0;
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
  }

  refreshColors() {
    const cs = getComputedStyle(this.canvas);
    this.c1 = cs.getPropertyValue('--accent').trim() || '#a493ff';
    this.c2 = cs.getPropertyValue('--accent2').trim() || '#4fd8ff';
  }

  _resize() {
    const dpr = Math.min(devicePixelRatio || 1, 2);
    const w = this.canvas.clientWidth, h = this.canvas.clientHeight;
    if (this.canvas.width !== Math.round(w * dpr) || this.canvas.height !== Math.round(h * dpr)) {
      this.canvas.width = Math.round(w * dpr);
      this.canvas.height = Math.round(h * dpr);
    }
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return [w, h];
  }

  /** Log-spaced band levels between 35 Hz and 16 kHz, 0..1. */
  _bands(n) {
    const an = this.player.analyser;
    const bins = an.frequencyBinCount;
    if (!this.freq || this.freq.length !== bins) this.freq = new Uint8Array(bins);
    an.getByteFrequencyData(this.freq);
    const nyq = (this.player.ctx?.sampleRate || 48000) / 2;
    if (!this.smooth || this.smooth.length !== n) this.smooth = new Float32Array(n);
    const lo = Math.log(35), hi = Math.log(16000);
    for (let i = 0; i < n; i++) {
      const f0 = Math.exp(lo + ((hi - lo) * i) / n), f1 = Math.exp(lo + ((hi - lo) * (i + 1)) / n);
      const b0 = Math.floor((f0 / nyq) * bins), b1 = Math.max(b0 + 1, Math.ceil((f1 / nyq) * bins));
      let m = 0;
      for (let b = b0; b < b1 && b < bins; b++) if (this.freq[b] > m) m = this.freq[b];
      const v = (m / 255) ** 1.6;
      this.smooth[i] = v > this.smooth[i] ? v : this.smooth[i] * 0.88 + v * 0.12;
    }
    return this.smooth;
  }

  frame() {
    const o = this.getOpts();
    const [w, h] = this._resize();
    const ctx = this.ctx;
    ctx.clearRect(0, 0, w, h);
    if (!this.player.analyser || o.style === 'none') return;
    const n = Math.max(16, Math.min(256, o.bars || 96));

    if (o.style === 'scope') {
      const an = this.player.analyser;
      if (!this.wave || this.wave.length !== an.fftSize) this.wave = new Uint8Array(an.fftSize);
      an.getByteTimeDomainData(this.wave);
      const mid = h * 0.78, amp = h * 0.16;
      const g = ctx.createLinearGradient(0, 0, w, 0);
      g.addColorStop(0, this.c1); g.addColorStop(1, this.c2);
      ctx.strokeStyle = g;
      ctx.lineWidth = 2;
      ctx.shadowColor = this.c1;
      ctx.shadowBlur = 12;
      ctx.beginPath();
      for (let i = 0; i < this.wave.length; i += 2) {
        const x = (i / (this.wave.length - 1)) * w;
        const y = mid + ((this.wave[i] - 128) / 128) * amp;
        i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      }
      ctx.stroke();
      ctx.shadowBlur = 0;
      return;
    }

    if (o.style === 'radial') {
      const half = Math.ceil(n / 2);
      const v = this._bands(half);
      const a = this.getAnchor?.()?.getBoundingClientRect();
      const c = this.canvas.getBoundingClientRect();
      if (!a || !a.width) return;
      const cx = a.left - c.left + a.width / 2, cy = a.top - c.top + a.height / 2;
      const round = this.canvas.closest('[data-shape="round"]');
      const r0 = (round ? a.width / 2 : (a.width / 2) * 1.08) + 10;
      const len = Math.min(a.width * 0.32, 140);
      const lw = Math.max(1.5, ((2 * Math.PI * r0) / (half * 2)) * 0.55);
      const g = ctx.createRadialGradient(cx, cy, r0, cx, cy, r0 + len);
      g.addColorStop(0, this.c1); g.addColorStop(1, this.c2);
      ctx.strokeStyle = g;
      ctx.lineWidth = lw;
      ctx.lineCap = 'round';
      ctx.beginPath();
      for (let i = 0; i < half * 2; i++) {
        const k = i < half ? i : half * 2 - 1 - i;
        const ang = (i / (half * 2)) * Math.PI * 2 - Math.PI / 2;
        const l = 2 + v[k] * len;
        const cos = Math.cos(ang), sin = Math.sin(ang);
        ctx.moveTo(cx + cos * r0, cy + sin * r0);
        ctx.lineTo(cx + cos * (r0 + l), cy + sin * (r0 + l));
      }
      ctx.stroke();
      return;
    }

    const v = this._bands(o.style === 'mirror' ? Math.ceil(n / 2) : n);
    const g = ctx.createLinearGradient(0, h, 0, h * 0.55);
    g.addColorStop(0, this.c1); g.addColorStop(1, this.c2);
    ctx.fillStyle = g;
    const maxH = h * 0.4;
    const total = o.style === 'mirror' ? v.length * 2 : v.length;
    const bw = w / total;
    ctx.beginPath();
    for (let i = 0; i < total; i++) {
      const k = o.style === 'mirror' ? (i < v.length ? v.length - 1 - i : i - v.length) : i;
      const bh = 2 + v[k] * maxH;
      const x = i * bw + bw * 0.15;
      if (ctx.roundRect) ctx.roundRect(x, h - bh, bw * 0.7, bh, Math.min(3, bw * 0.35));
      else ctx.rect(x, h - bh, bw * 0.7, bh);
    }
    ctx.fill();
  }
}
