#!/usr/bin/env node
/**
 * Minimal static dev server with HTTP Range support (needed for seeking in
 * large FLAC files; python's http.server and `php -S` do not support Range).
 *
 * Usage: node website/tools/serve.mjs [port] [dir]   (dir defaults to dist-website/)   (default 8080, binds to 0.0.0.0 so you
 * can test from a phone in the same network)
 */
import http from 'node:http';
import { createReadStream } from 'node:fs';
import { stat } from 'node:fs/promises';
import path from 'node:path';
import { networkInterfaces } from 'node:os';
import { fileURLToPath } from 'node:url';
const ROOT = path.resolve(process.argv[3] || path.join(path.dirname(fileURLToPath(import.meta.url)), '../../dist-website'));

const port = Number(process.argv[2]) || 8080;
const MIME = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8', '.json': 'application/json; charset=utf-8', '.svg': 'image/svg+xml',
  '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.png': 'image/png', '.webp': 'image/webp', '.ico': 'image/x-icon',
  '.flac': 'audio/flac', '.ttf': 'font/ttf', '.zip': 'application/zip', '.7z': 'application/x-7z-compressed',
  '.md': 'text/markdown; charset=utf-8', '.txt': 'text/plain; charset=utf-8', '.webmanifest': 'application/manifest+json',
};

http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, 'http://x');
    let file = path.normalize(path.join(ROOT, decodeURIComponent(url.pathname)));
    if (!file.startsWith(ROOT)) { res.writeHead(403).end(); return; }
    let st = await stat(file).catch(() => null);
    if (st?.isDirectory()) { file = path.join(file, 'index.html'); st = await stat(file).catch(() => null); }
    if (!st) { res.writeHead(404, { 'content-type': 'text/plain' }).end('Not found'); return; }

    const headers = {
      'content-type': MIME[path.extname(file).toLowerCase()] || 'application/octet-stream',
      'accept-ranges': 'bytes',
      'cache-control': /\.(html|js|css|json)$/.test(file) ? 'no-cache' : 'public, max-age=3600',
    };
    const range = req.headers.range?.match(/bytes=(\d*)-(\d*)/);
    if (range) {
      let start = range[1] ? +range[1] : st.size - +range[2];
      let end = range[1] && range[2] ? +range[2] : st.size - 1;
      if (start >= st.size || start > end) {
        res.writeHead(416, { 'content-range': `bytes */${st.size}` }).end();
        return;
      }
      end = Math.min(end, st.size - 1);
      res.writeHead(206, { ...headers, 'content-range': `bytes ${start}-${end}/${st.size}`, 'content-length': end - start + 1 });
      if (req.method === 'HEAD') return res.end();
      createReadStream(file, { start, end }).pipe(res);
    } else {
      res.writeHead(200, { ...headers, 'content-length': st.size });
      if (req.method === 'HEAD') return res.end();
      createReadStream(file).pipe(res);
    }
  } catch (e) {
    res.writeHead(500).end(String(e));
  }
}).listen(port, '0.0.0.0', () => {
  const ips = Object.values(networkInterfaces()).flat().filter(i => i?.family === 'IPv4' && !i.internal).map(i => i.address);
  console.log(`Serving ${ROOT}\n  http://localhost:${port}/${ips.map(ip => `\n  http://${ip}:${port}/`).join('')}`);
});
