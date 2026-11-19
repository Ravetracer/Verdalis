// Canvas waveform renderer with multiple styles, seeking, hover and drag.
import { clamp, fmtTime } from './util.js';

export const WAVE_DEFAULTS = {
  style: 'mirror',          // see STYLE_NAMES
  height: 40,               // px (container height)
  barWidth: 2,
  barGap: 1,
  barRadius: 1,
  minHeight: 1,             // px, minimum bar height
  gain: 1,                  // amplitude multiplier
  normalize: true,          // scale to the loudest peak of the track
  scale: 'sqrt',            // linear | sqrt | log
  resolution: 2,            // px per point for continuous styles (line, wave)
  lineWidth: 1.5,           // line style
  blockHeight: 3,           // blocks style
  reflection: 0.3,          // reflection style: share of height used for the mirror image
  slimHeight: 4,            // slim style
  color: 'rgba(255,255,255,0.22)',
  progressColor: ['accent', 'accent2'], // color or list of gradient stops
  gradientDirection: 'horizontal',       // horizontal | vertical
  hoverColor: 'rgba(255,255,255,0.45)',
  bufferedColor: 'rgba(255,255,255,0.32)',
  cursor: true,
  cursorColor: '#ffffff',
  cursorWidth: 1.5,
  hoverTime: true,
  animate: true,
  interactive: true,
};

const rr = (ctx, x, y, w, h, r) => {
  if (r > 0 && ctx.roundRect) ctx.roundRect(x, y, w, h, Math.max(0, Math.min(r, w / 2, h / 2)));
  else ctx.rect(x, y, w, h);
};

/** Style renderers: draw the full waveform with the current fill/stroke style. */
export const STYLES = {
  bars: {
    label: 'Bars',
    draw(ctx, c, w, h, o, k) {
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) {
        const bh = Math.max(o.minHeight, c.peak[i] * h * k);
        rr(ctx, i * c.step, h - bh, o.barWidth, bh, o.barRadius);
      }
      ctx.fill();
    },
  },
  mirror: {
    label: 'Mirrored bars',
    draw(ctx, c, w, h, o, k) {
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) {
        const bh = Math.max(o.minHeight, c.peak[i] * h * k);
        rr(ctx, i * c.step, (h - bh) / 2, o.barWidth, bh, o.barRadius);
      }
      ctx.fill();
    },
  },
  reflection: {
    label: 'Bars + reflection',
    draw(ctx, c, w, h, o, k) {
      const split = Math.round(h * (1 - o.reflection));
      const ratio = (h - split - 1) / split;
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) {
        const bh = Math.max(o.minHeight, c.peak[i] * (split - 1) * k);
        rr(ctx, i * c.step, split - bh, o.barWidth, bh, o.barRadius);
      }
      ctx.fill();
      ctx.globalAlpha *= 0.38;
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) {
        const bh = Math.max(o.minHeight, c.peak[i] * (split - 1) * k) * ratio;
        rr(ctx, i * c.step, split + 1, o.barWidth, bh, o.barRadius);
      }
      ctx.fill();
    },
  },
  rms: {
    label: 'Peak + RMS',
    draw(ctx, c, w, h, o, k) {
      const a = ctx.globalAlpha;
      ctx.globalAlpha = a * 0.45;
      STYLES.mirror.draw(ctx, c, w, h, o, k);
      ctx.globalAlpha = a;
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) {
        const bh = Math.max(o.minHeight, c.rms[i] * h * k);
        rr(ctx, i * c.step, (h - bh) / 2, o.barWidth, bh, o.barRadius);
      }
      ctx.fill();
    },
  },
  blocks: {
    label: 'LED blocks',
    draw(ctx, c, w, h, o, k) {
      const seg = Math.max(1, o.blockHeight), g = 1;
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) {
        const count = Math.max(1, Math.round((c.peak[i] * h * k) / (seg + g)));
        for (let j = 0; j < count; j++) ctx.rect(i * c.step, h - (j + 1) * (seg + g) + g, o.barWidth, seg);
      }
      ctx.fill();
    },
  },
  dots: {
    label: 'Dots',
    draw(ctx, c, w, h, o, k) {
      const r = Math.max(0.75, o.barWidth / 2), sp = o.barWidth + Math.max(1, o.barGap), mid = h / 2;
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) {
        const cx = i * c.step + r;
        const count = Math.max(0, Math.round((c.peak[i] * mid * k) / sp));
        for (let j = -count; j <= count; j++) {
          const cy = mid + j * sp;
          ctx.moveTo(cx + r, cy);
          ctx.arc(cx, cy, r, 0, Math.PI * 2);
        }
      }
      ctx.fill();
    },
  },
  needles: {
    label: 'Needles',
    draw(ctx, c, w, h, o, k) {
      const r = Math.max(1, o.barWidth / 2), lw = Math.max(1, o.barWidth / 3), mid = h / 2;
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) {
        const cx = i * c.step + o.barWidth / 2;
        const half = Math.max(r, c.peak[i] * (mid - r) * k);
        ctx.rect(cx - lw / 2, mid - half, lw, half * 2);
        ctx.moveTo(cx + r, mid - half);
        ctx.arc(cx, mid - half, r, 0, Math.PI * 2);
        ctx.moveTo(cx + r, mid + half);
        ctx.arc(cx, mid + half, r, 0, Math.PI * 2);
      }
      ctx.fill();
    },
  },
  line: {
    label: 'Outline',
    continuous: true,
    draw(ctx, c, w, h, o, k) {
      const mid = h / 2, lw = o.lineWidth, amp = mid - lw;
      const x = i => (c.n > 1 ? (i / (c.n - 1)) * w : 0);
      ctx.lineWidth = lw;
      ctx.lineJoin = 'round';
      ctx.beginPath();
      for (let i = 0; i < c.n; i++) ctx.lineTo(x(i), mid - c.peak[i] * amp * k);
      for (let i = c.n - 1; i >= 0; i--) ctx.lineTo(x(i), mid + c.peak[i] * amp * k);
      ctx.closePath();
      ctx.stroke();
    },
  },
  wave: {
    label: 'Smooth wave',
    continuous: true,
    draw(ctx, c, w, h, o, k) {
      const mid = h / 2;
      const x = i => (c.n > 1 ? (i / (c.n - 1)) * w : 0);
      const y = (i, s) => mid + s * Math.max(0.5, c.peak[i] * mid * k);
      ctx.beginPath();
      ctx.moveTo(0, y(0, -1));
      for (let i = 1; i < c.n; i++) {
        const xm = (x(i - 1) + x(i)) / 2, ym = (y(i - 1, -1) + y(i, -1)) / 2;
        ctx.quadraticCurveTo(x(i - 1), y(i - 1, -1), xm, ym);
      }
      ctx.lineTo(w, y(c.n - 1, -1));
      ctx.lineTo(w, y(c.n - 1, 1));
      for (let i = c.n - 2; i >= 0; i--) {
        const xm = (x(i + 1) + x(i)) / 2, ym = (y(i + 1, 1) + y(i, 1)) / 2;
        ctx.quadraticCurveTo(x(i + 1), y(i + 1, 1), xm, ym);
      }
      ctx.lineTo(0, y(0, 1));
      ctx.closePath();
      ctx.fill();
    },
  },
  slim: {
    label: 'Slim bar (no waveform)',
    draw(ctx, c, w, h, o) {
      const th = Math.min(h, o.slimHeight);
      ctx.beginPath();
      rr(ctx, 0, (h - th) / 2, w, th, th / 2);
      ctx.fill();
    },
  },
};
export const STYLE_NAMES = Object.keys(STYLES);

// ---------------------------------------------------------------- peaks loading

const cache = new Map();

export function loadPeaks(url) {
  if (!url) return Promise.resolve(null);
  if (!cache.has(url)) {
    cache.set(url, fetch(url)
      .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
      .then(decodePeaks)
      .catch(e => { cache.delete(url); console.warn('peaks', url, e); return null; }));
  }
  return cache.get(url);
}

function decodePeaks(j) {
  const dec = s => {
    const b = atob(s), out = new Float32Array(b.length);
    for (let i = 0; i < b.length; i++) {
      const v = b.charCodeAt(i) / 255;
      out[i] = j.enc === 'sqrt' ? v * v : v;
    }
    return out;
  };
  const peak = dec(j.peak), rms = dec(j.rms);
  let max = 0;
  for (const v of peak) if (v > max) max = v;
  return { peak, rms, max: max || 1, duration: j.duration };
}

// ---------------------------------------------------------------- component

const instances = new Set();
const ro = typeof ResizeObserver !== 'undefined'
  ? new ResizeObserver(entries => entries.forEach(e => e.target._wave?.resize()))
  : null;

const shapeFn = { linear: v => v, sqrt: Math.sqrt, log: v => (v <= 0 ? 0 : Math.max(0, 1 + (20 * Math.log10(v)) / 48)) };

export class Waveform {
  /**
   * @param {HTMLElement} host  container (its height defines the waveform height)
   * @param {object} opts       see WAVE_DEFAULTS
   * @param {{onSeek?:(f:number)=>void, getDuration?:()=>number, label?:string}} hooks
   */
  constructor(host, opts = {}, hooks = {}) {
    this.host = host;
    this.hooks = hooks;
    this.data = null;
    this.progress = 0;
    this.buffered = 0;
    this.hover = null;
    this.drag = null;
    this.anim = 1;
    host.classList.add('wf');
    host.textContent = '';
    host._wave = this;
    this.canvas = document.createElement('canvas');
    host.appendChild(this.canvas);
    this.ctx = this.canvas.getContext('2d');
    this.setOptions(opts);
    this._bind();
    instances.add(this);
    ro?.observe(host);
    this.resize();
  }

  static sweep() {
    for (const w of instances) if (!w.host.isConnected) w.destroy();
  }

  destroy() {
    ro?.unobserve(this.host);
    instances.delete(this);
    cancelAnimationFrame(this._raf);
    delete this.host._wave;
  }

  setOptions(opts) {
    this.o = { ...WAVE_DEFAULTS, ...opts };
    if (this.o.height) this.host.style.height = `${this.o.height}px`;
    this.host.dataset.style = this.o.style;
    this.cols = null;
    this._colors = null;
    this.host.classList.toggle('wf-interactive', !!this.o.interactive);
    this.resize(true);
  }

  setData(data, animate = this.o.animate) {
    this.data = data;
    this.cols = null;
    if (data && animate && !matchMedia('(prefers-reduced-motion: reduce)').matches) {
      const t0 = performance.now();
      const step = now => {
        const t = Math.min(1, (now - t0) / 650);
        this.anim = 1 - (1 - t) ** 3;
        this.draw();
        if (t < 1) this._raf = requestAnimationFrame(step);
      };
      this.anim = 0;
      cancelAnimationFrame(this._raf);
      this._raf = requestAnimationFrame(step);
    } else {
      this.anim = 1;
      this.draw();
    }
  }

  setProgress(p, buffered = this.buffered) {
    p = clamp(p || 0, 0, 1);
    buffered = clamp(buffered || 0, 0, 1);
    // Redraw on sub-pixel movement (in device pixels) so slow progress on long tracks glides instead of stepping.
    const px = this.w * (this.dpr || 1);
    // Compare against the last drawn state, not the last call, so tiny per-frame steps accumulate.
    const moved = Math.abs((p - (this._drawnP ?? -1)) * px) >= 0.1 || Math.abs((buffered - (this._drawnB ?? -1)) * this.w) >= 2;
    this.progress = p;
    this.buffered = buffered;
    if (moved) {
      this.host.setAttribute('aria-valuenow', String(Math.round(p * 100)));
      this.draw();
    }
  }

  refreshColors() { this._colors = null; this.draw(); }

  resize(force) {
    const r = this.host.getBoundingClientRect();
    const w = Math.round(r.width), h = Math.round(r.height);
    const dpr = Math.min(window.devicePixelRatio || 1, 3);
    if (!force && w === this.w && h === this.h && dpr === this.dpr) return;
    this.w = w; this.h = h; this.dpr = dpr;
    this.canvas.width = Math.max(1, Math.round(w * dpr));
    this.canvas.height = Math.max(1, Math.round(h * dpr));
    this.cols = null;
    this._colors = null;
    this.draw();
  }

  _bind() {
    const el = this.host;
    const frac = e => {
      const r = el.getBoundingClientRect();
      return clamp((e.clientX - r.left) / r.width, 0, 1);
    };
    el.addEventListener('pointerdown', e => {
      if (!this.o.interactive || e.button !== 0) return;
      this.drag = frac(e);
      try { el.setPointerCapture(e.pointerId); } catch { /* ignore */ }
      this.draw();
    });
    el.addEventListener('pointermove', e => {
      if (!this.o.interactive) return;
      if (this.drag != null) { this.drag = frac(e); this.draw(); }
      else if (e.pointerType === 'mouse') { this.hover = frac(e); this.draw(); }
    });
    el.addEventListener('pointerup', e => {
      if (this.drag == null) return;
      const f = this.drag;
      this.drag = null;
      this.hover = e.pointerType === 'mouse' ? f : null;
      this.progress = f;
      this.hooks.onSeek?.(f);
      this.draw();
    });
    el.addEventListener('pointercancel', () => { this.drag = null; this.draw(); });
    el.addEventListener('pointerleave', () => { if (this.hover != null) { this.hover = null; this.draw(); } });
    el.addEventListener('click', e => e.stopPropagation());
    el.addEventListener('keydown', e => {
      if (!this.o.interactive) return;
      const d = this.hooks.getDuration?.() || 0;
      const step = d ? (e.shiftKey ? 30 : 5) / d : 0.02;
      let f = null;
      if (e.key === 'ArrowRight') f = this.progress + step;
      else if (e.key === 'ArrowLeft') f = this.progress - step;
      else if (e.key === 'Home') f = 0;
      else if (e.key === 'End') f = 0.999;
      if (f == null) return;
      e.preventDefault();
      e.stopPropagation();
      this.hooks.onSeek?.(clamp(f, 0, 1));
    });
    if (this.o.interactive) {
      el.tabIndex = 0;
      el.setAttribute('role', 'slider');
      el.setAttribute('aria-label', this.hooks.label || 'Seek');
      el.setAttribute('aria-valuemin', '0');
      el.setAttribute('aria-valuemax', '100');
      el.setAttribute('aria-valuenow', '0');
    }
  }

  _color(v) {
    if (typeof v !== 'string') return v;
    const name = v === 'accent' || v === 'accent2' ? `--${v}` : (v.match(/^var\((--[\w-]+)\)$/) || v.match(/^(--[\w-]+)$/))?.[1];
    if (!name) return v;
    return getComputedStyle(this.host).getPropertyValue(name).trim() || '#ffffff';
  }

  _paints() {
    if (this._colors) return this._colors;
    const o = this.o, ctx = this.ctx;
    let progress;
    if (Array.isArray(o.progressColor) && o.progressColor.length > 1) {
      const g = o.gradientDirection === 'vertical' ? ctx.createLinearGradient(0, 0, 0, this.h) : ctx.createLinearGradient(0, 0, this.w, 0);
      o.progressColor.forEach((c, i, a) => g.addColorStop(i / (a.length - 1), this._color(c)));
      progress = g;
    } else {
      progress = this._color(Array.isArray(o.progressColor) ? o.progressColor[0] : o.progressColor);
    }
    return (this._colors = {
      base: this._color(o.color),
      progress,
      hover: this._color(o.hoverColor),
      buffered: this._color(o.bufferedColor),
      cursor: this._color(o.cursorColor),
    });
  }

  _columns(style) {
    if (this.cols) return this.cols;
    const o = this.o, w = this.w;
    const step = style.continuous ? Math.max(1, o.resolution) : Math.max(1, o.barWidth + o.barGap);
    const n = Math.max(2, Math.floor((w + (style.continuous ? 0 : o.barGap)) / step));
    const peak = new Float32Array(n), rms = new Float32Array(n);
    if (this.data) {
      const src = this.data.peak, srcR = this.data.rms, len = src.length;
      const shape = shapeFn[o.scale] || shapeFn.linear;
      const norm = o.normalize ? this.data.max : 1;
      for (let i = 0; i < n; i++) {
        const a = Math.floor((i * len) / n), b = Math.max(a + 1, Math.floor(((i + 1) * len) / n));
        let m = 0, s = 0;
        for (let j = a; j < b; j++) { if (src[j] > m) m = src[j]; s += srcR[j]; }
        peak[i] = Math.min(1, shape(m / norm) * o.gain);
        rms[i] = Math.min(1, shape(s / (b - a) / norm) * o.gain);
      }
    } else {
      peak.fill(0.04);
      rms.fill(0.02);
    }
    return (this.cols = { peak, rms, n, step });
  }

  draw() {
    const { ctx, w, h } = this;
    if (!w || !h) return;
    const o = this.o;
    const style = STYLES[o.style] || STYLES.mirror;
    const cols = this._columns(style);
    const paints = this._paints();
    const prog = (this.drag ?? this.progress) * w;
    this._drawnP = this.progress;
    this._drawnB = this.buffered;
    const k = this.anim;

    ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);

    const layer = (fill, x0, x1) => {
      if (x1 - x0 <= 0) return;
      ctx.save();
      ctx.beginPath();
      ctx.rect(x0, 0, x1 - x0, h);
      ctx.clip();
      ctx.fillStyle = fill;
      ctx.strokeStyle = fill;
      style.draw(ctx, cols, w, h, o, k);
      ctx.restore();
    };

    layer(paints.base, 0, w);
    if (o.bufferedColor && this.buffered * w > prog) layer(paints.buffered, prog, this.buffered * w);
    const hx = this.drag == null && this.hover != null ? this.hover * w : null;
    if (hx != null && hx > prog) layer(paints.hover, prog, hx);
    // Fill whole device pixels, then blend the partially covered pixel column by its coverage.
    const dpr = this.dpr || 1, pd = prog * dpr, full = Math.floor(pd) / dpr, part = pd - Math.floor(pd);
    layer(paints.progress, 0, full);
    if (part > 0.01) {
      ctx.globalAlpha = part;
      layer(paints.progress, full, full + 1 / dpr);
      ctx.globalAlpha = 1;
    }

    if (o.cursor && (this.progress > 0 || this.drag != null)) {
      ctx.fillStyle = paints.cursor;
      ctx.fillRect(prog - o.cursorWidth / 2, 0, o.cursorWidth, h);
    }
    if (hx != null) {
      ctx.fillStyle = paints.hover;
      ctx.fillRect(Math.round(hx), 0, 1, h);
    }

    const tf = this.drag ?? (hx != null ? this.hover : null);
    const dur = this.hooks.getDuration?.() || 0;
    if (o.hoverTime && tf != null && dur && h >= 22) {
      const text = fmtTime(tf * dur);
      ctx.font = '600 11px system-ui, sans-serif';
      const tw = ctx.measureText(text).width + 12;
      const x = clamp(tf * w - tw / 2, 0, w - tw);
      ctx.fillStyle = 'rgba(8,7,18,.85)';
      ctx.beginPath();
      rr(ctx, x, 1, tw, 17, 8.5);
      ctx.fill();
      ctx.fillStyle = '#fff';
      ctx.textBaseline = 'middle';
      ctx.fillText(text, x + 6, 10);
    }
  }
}
