// Small DOM, formatting and storage helpers.

export const $ = (sel, root = document) => root.querySelector(sel);
export const $$ = (sel, root = document) => [...root.querySelectorAll(sel)];

export const esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);

export const icon = (name, cls = '') => `<svg class="i ${cls}" aria-hidden="true"><use href="#i-${name}"/></svg>`;

export const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));

export const isTouch = () => matchMedia('(hover: none) and (pointer: coarse)').matches;

const pad = n => String(n).padStart(2, '0');

export function fmtTime(s) {
  if (!isFinite(s) || s < 0) s = 0;
  s = Math.floor(s);
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60;
  return h ? `${h}:${pad(m)}:${pad(sec)}` : `${m}:${pad(sec)}`;
}

export function fmtDuration(s) {
  const min = Math.round(s / 60);
  if (min < 60) return `${min} min`;
  return `${Math.floor(min / 60)} h ${min % 60} min`;
}

export function fmtDate(str, locale = 'en-US') {
  if (!str) return '';
  if (/^\d{4}$/.test(str)) return str;
  const d = new Date(`${str.slice(0, 10)}T12:00:00`);
  if (isNaN(d)) return str;
  return d.toLocaleDateString(locale, { year: 'numeric', month: 'long', day: 'numeric' });
}

export function fmtBytes(b) {
  if (!b) return '';
  const u = ['B', 'KB', 'MB', 'GB'];
  let i = 0;
  while (b >= 1024 && i < u.length - 1) { b /= 1024; i++; }
  return `${b.toFixed(i > 1 ? 1 : 0)} ${u[i]}`;
}

/** Deep merge of plain objects; arrays and primitives replace. */
export function merge(...objs) {
  const out = {};
  for (const o of objs) {
    if (!o || typeof o !== 'object') continue;
    for (const [k, v] of Object.entries(o)) {
      if (v === undefined) continue;
      out[k] = v && typeof v === 'object' && !Array.isArray(v) && out[k] && typeof out[k] === 'object' && !Array.isArray(out[k])
        ? merge(out[k], v) : v;
    }
  }
  return out;
}

export const store = {
  get(key, fallback) {
    try { const v = localStorage.getItem(`vd.${key}`); return v == null ? fallback : JSON.parse(v); } catch { return fallback; }
  },
  set(key, value) {
    try { value == null ? localStorage.removeItem(`vd.${key}`) : localStorage.setItem(`vd.${key}`, JSON.stringify(value)); } catch { /* private mode */ }
  },
};

// ---------------------------------------------------------------- markdown-lite

const safeUrl = u => (/^\s*(javascript|data|vbscript):/i.test(u) ? '#' : u);

function inline(s) {
  return esc(s)
    .replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, (_, t, u) => {
      const ext = /^(https?:)?\/\//.test(u);
      return `<a href="${safeUrl(u)}"${ext ? ' target="_blank" rel="noopener"' : ''}>${t}</a>`;
    })
    .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
    .replace(/(^|[^*])\*([^*\s][^*]*)\*/g, '$1<em>$2</em>');
}

/**
 * Renders "markdown-lite": a string (blocks separated by blank lines) or an
 * array of blocks. Supports paragraphs, line breaks, **bold**, *italic*,
 * [links](url), "- " lists, "# " headings and "> " quotes.
 */
export function md(src) {
  const blocks = Array.isArray(src) ? src : String(src || '').split(/\n\s*\n/);
  return blocks.map(b => {
    b = String(b).trim();
    if (!b) return '';
    const lines = b.split('\n');
    if (lines.every(l => /^\s*[-*]\s+/.test(l))) {
      return `<ul>${lines.map(l => `<li>${inline(l.replace(/^\s*[-*]\s+/, ''))}</li>`).join('')}</ul>`;
    }
    const hm = lines.length === 1 && b.match(/^(#{1,3})\s+(.*)$/);
    if (hm) return `<h${hm[1].length + 2}>${inline(hm[2])}</h${hm[1].length + 2}>`;
    if (lines.every(l => l.startsWith('>'))) {
      return `<blockquote>${lines.map(l => inline(l.replace(/^>\s?/, ''))).join('<br>')}</blockquote>`;
    }
    return `<p>${lines.map(inline).join('<br>')}</p>`;
  }).join('');
}

/** Plain-text excerpt of the first block(s). */
export function excerpt(src, max = 260) {
  const blocks = Array.isArray(src) ? src : String(src || '').split(/\n\s*\n/);
  let out = '';
  for (const b of blocks) {
    const t = String(b).replace(/\[([^\]]+)\]\([^)]+\)/g, '$1').replace(/[*#>]/g, '').replace(/\s+/g, ' ').trim();
    if (!t) continue;
    out += (out ? ' ' : '') + t;
    if (out.length > max * 0.6) break;
  }
  return out.length > max ? `${out.slice(0, max).replace(/\s+\S*$/, '')}…` : out;
}

export function toast(msg) {
  const root = document.getElementById('toasts');
  if (!root) return;
  const el = document.createElement('div');
  el.className = 'toast';
  el.textContent = msg;
  root.appendChild(el);
  requestAnimationFrame(() => el.classList.add('show'));
  setTimeout(() => { el.classList.remove('show'); setTimeout(() => el.remove(), 400); }, 2400);
}

export async function share({ title, url }) {
  try {
    if (navigator.share && isTouch()) { await navigator.share({ title, url }); return; }
    await navigator.clipboard.writeText(url);
    toast('Link copied to clipboard');
  } catch (e) {
    if (e?.name !== 'AbortError') toast(url);
  }
}
