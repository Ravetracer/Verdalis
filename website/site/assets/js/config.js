// Default configuration. site.json is merged on top of this, and the album
// "player" block in album.json is merged on top of that for its album.
import { WAVE_DEFAULTS } from './waveform.js';
import { merge, isTouch } from './util.js';

export const DEFAULT_CONFIG = {
  siteTitle: 'My Music',
  tagline: '',
  artist: '',
  description: '',
  logo: '',
  background: '',
  locale: 'en-US',
  links: [],
  footer: '',
  about: null,
  theme: {
    accent: '#a493ff',
    accent2: '#4fd8ff',
    bg: '#05040c',
    text: '#ecebf5',
    radius: 14,
    backgroundDim: 0.55,
    useAlbumAccent: true,
  },
  home: { featureLatest: true, showFilters: true, gridMin: 200 },
  player: {
    autoAdvance: true,
    rememberPosition: true,
    trackDownloads: true,
    continueToNextAlbum: false,
    defaultQuality: 'hq',
    visualizer: { enabled: true, onMobile: false, style: 'radial', bars: 96, smoothing: 0.82, opacity: 0.75 },
    nowPlaying: { coverShape: 'rounded', spin: false },
    trackList: { layout: 'waveform' },
    waveform: { ...WAVE_DEFAULTS },
    waveformBar: { height: 40 },
    waveformBarMobile: { style: 'bars', height: 18, cursor: false, hoverTime: false },
    waveformTrackList: { height: 32, cursor: false },
    waveformNowPlaying: { style: 'reflection', height: 88 },
  },
};

export const WAVE_CONTEXTS = {
  bar: 'waveformBar',
  barMobile: 'waveformBarMobile',
  trackList: 'waveformTrackList',
  nowPlaying: 'waveformNowPlaying',
};

/** Effective waveform options for a context, including album overrides. */
export function waveOptions(config, context, album) {
  const key = WAVE_CONTEXTS[context];
  const p = config.player, ap = album?.player || {};
  const o = merge(WAVE_DEFAULTS, p.waveform, p[key], ap.waveform, ap[key]);
  if (context === 'barMobile' && o.height < 12) o.interactive = false;
  return o;
}

export function playerConfig(config, album) {
  return merge(config.player, album?.player);
}

export const visualizerAllowed = cfg =>
  !!cfg.visualizer?.enabled && cfg.visualizer.style !== 'none' && (!isTouch() || cfg.visualizer.onMobile)
  && !!(window.AudioContext || window.webkitAudioContext);
