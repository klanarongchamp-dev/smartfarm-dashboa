// SmartFarm V5.1 Service Worker

self.addEventListener('install', (event) => {
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  console.log('Service Worker Activated');
});

self.addEventListener('fetch', (event) => {
  // Ready for offline cache
});
