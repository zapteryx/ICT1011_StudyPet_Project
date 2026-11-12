// Scope-relative assets work both at / and at a hosted /project/pwa/ path.
const CACHE_NAME = 'studypet-v1.1.0';
const ASSETS = ['./', 'index.html', 'styles.css', 'app.js?v=1.1', 'ble.js?v=1.1',
  'manifest.json', 'icon-192.svg', 'icon-512.svg'];
const scoped = path => new URL(path, self.registration.scope).href;

self.addEventListener('install', event => {
  event.waitUntil(caches.open(CACHE_NAME)
    .then(cache => cache.addAll(ASSETS.map(scoped)))
    .then(() => self.skipWaiting()));
});

self.addEventListener('activate', event => {
  event.waitUntil((async () => {
    for (const name of await caches.keys()) {
      if (name.startsWith('studypet-') && name !== CACHE_NAME) await caches.delete(name);
    }
    await self.clients.claim();
  })());
});

// Prefer published fixes online; keep a scoped offline copy as fallback.
self.addEventListener('fetch', event => {
  if (event.request.method !== 'GET' || new URL(event.request.url).origin !== self.location.origin) return;
  event.respondWith((async () => {
    const cache = await caches.open(CACHE_NAME);
    try {
      const response = await fetch(event.request, { cache: 'no-cache' });
      if (response.ok) await cache.put(event.request, response.clone());
      return response;
    } catch (error) {
      const cached = await cache.match(event.request);
      if (cached) return cached;
      if (event.request.mode === 'navigate') return (await cache.match(scoped('index.html'))) || Response.error();
      return Response.error();
    }
  })());
});
