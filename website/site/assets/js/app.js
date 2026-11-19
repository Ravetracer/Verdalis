// Bootstrap and hash router. The audio element lives outside the routed view,
// so navigating between plugin pages never interrupts a playing demo.
import { $, $$, esc, merge, share } from './util.js';
import { DEFAULT_CONFIG } from './config.js';
import { Player } from './player.js';
import { PlayerUI } from './ui.js';
import { Waveform } from './waveform.js';
import { renderHome, renderPlugin, renderNotFound } from './views.js';

async function fetchJson(url) {
  const r = await fetch(url, { cache: 'no-cache' });
  if (!r.ok) throw new Error(`${url}: HTTP ${r.status}`);
  return r.json();
}

function applyTheme(c) {
  const t = c.theme, s = document.documentElement.style;
  s.setProperty('--accent', t.accent);
  s.setProperty('--accent2', t.accent2);
  s.setProperty('--bg', t.bg);
  s.setProperty('--text', t.text);
  s.setProperty('--radius', `${t.radius}px`);
}

// Sections of the home page that have their own route.
const HOME_SECTIONS = new Set(['suite', 'download', 'beyond', 'about']);

async function boot() {
  const view = $('#view');
  let site, catalog;
  try {
    [site, catalog] = await Promise.all([fetchJson('site.json'), fetchJson('data/catalog.json')]);
  } catch (e) {
    view.innerHTML = `<section class="empty"><h1 class="page-title">Could not load the catalog</h1><p>${esc(e.message)}</p><p>Serve the folder over HTTP (<code>node tools/serve.mjs</code>).</p></section>`;
    return;
  }

  const config = merge(DEFAULT_CONFIG, site);
  applyTheme(config);

  const albums = catalog.plugins;
  const bySlug = new Map();
  for (const a of albums) {
    a.trackById = new Map(a.tracks.map(t => [t.id, t]));
    bySlug.set(a.slug, a);
  }
  for (const a of albums) for (const alias of a.aliases || []) if (!bySlug.has(alias)) bySlug.set(alias, a);

  const app = { config, site, catalog, albums, bySlug, state: {} };
  app.share = p => share({ title: `${p.name} – ${p.tagline}`, url: `${location.origin}${location.pathname}#/plugin/${p.slug}` });
  app.player = new Player(app);
  app.ui = new PlayerUI(app);

  // ------------------------------------------------------------ router
  let current = null, dispose = null;

  function route(restoreScroll = false) {
    const hash = location.hash || '#/';
    if (hash === current) return;
    current = hash;
    const [name, ...rest] = hash.replace(/^#\/?/, '').split('/').map(s => decodeURIComponent(s));
    dispose?.();
    dispose = null;
    view.classList.remove('view-in');

    let res, section = null;
    if (!name || HOME_SECTIONS.has(name)) {
      section = name || null;
      res = renderHome(app, view, section);
    } else if (name === 'plugin' && bySlug.get(rest[0])) {
      const p = bySlug.get(rest[0]);
      if (p.slug !== rest[0]) history.replaceState(history.state, '', `#/plugin/${p.slug}${rest[1] ? `/${rest[1]}` : ''}`);
      res = renderPlugin(app, view, p, parseInt(rest[1], 10) || 0);
    } else res = renderNotFound(app, view);

    dispose = res?.dispose || null;
    document.title = res?.title || 'Verdalis';
    const navKey = name === 'plugin' ? (bySlug.get(rest[0])?.group === 'other' ? 'beyond' : 'suite') : (name || 'home');
    for (const a of $$('[data-nav]')) a.classList.toggle('active', a.dataset.nav === navKey);
    Waveform.sweep();
    app.ui.syncMarkers();
    void view.offsetWidth;
    view.classList.add('view-in');
    if (!section && !/\/\d+$/.test(hash)) window.scrollTo(0, restoreScroll ? history.state?.scroll || 0 : 0);
  }

  app.navigate = href => {
    if (href === (location.hash || '#/')) {
      const sec = href.replace(/^#\//, '');
      if (HOME_SECTIONS.has(sec)) $(`#${sec}`)?.scrollIntoView({ behavior: 'smooth', block: 'start' });
      else window.scrollTo({ top: 0, behavior: 'smooth' });
      return;
    }
    history.replaceState({ ...history.state, np: false, scroll: scrollY }, '');
    history.pushState({}, '', href);
    route(false);
  };

  document.addEventListener('click', e => {
    const a = e.target.closest('a[href^="#/"]');
    if (!a || e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || a.target) return;
    e.preventDefault();
    app.navigate(a.getAttribute('href'));
  });
  addEventListener('popstate', () => {
    if (app.ui.onPopState()) return;
    route(true);
  });
  addEventListener('hashchange', () => route(false));
  history.scrollRestoration = 'manual';

  app.player.restore();
  route(true);
  window.app = app; // handy for debugging in the console
}

boot();
