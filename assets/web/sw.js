// Lets the phone install the page as an app. Everything still comes live from
// the desktop; nothing is cached for offline use.
self.addEventListener("install", () => self.skipWaiting());
self.addEventListener("activate", event => event.waitUntil(self.clients.claim()));
self.addEventListener("fetch", event => event.respondWith(fetch(event.request)));
