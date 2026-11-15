// Only the public shell is cached. Prompts, files, terminal, cookies and APIs never are.
const CACHE='jlinux-connect-shell-v3';
const SHELL=['/','/app.js','/codex.js','/style.css','/icon.svg','/icon.png','/manifest.webmanifest'];
self.addEventListener('install',event=>event.waitUntil(caches.open(CACHE).then(cache=>cache.addAll(SHELL)).then(()=>self.skipWaiting())));
self.addEventListener('activate',event=>event.waitUntil(caches.keys().then(keys=>Promise.all(keys.filter(key=>key.startsWith('jlinux-connect-shell-')&&key!==CACHE).map(key=>caches.delete(key)))).then(()=>self.clients.claim())));
self.addEventListener('fetch',event=>{
 const url=new URL(event.request.url);
 if(event.request.method!=='GET'||url.origin!==self.location.origin||!SHELL.includes(url.pathname))return;
 event.respondWith(fetch(event.request).then(response=>{
  if(response.ok){const copy=response.clone();event.waitUntil(caches.open(CACHE).then(cache=>cache.put(url.pathname,copy)));}
  if(response.status>=500)return caches.match(url.pathname).then(cached=>cached||response);
  return response;
 }).catch(()=>caches.match(url.pathname).then(response=>response||Response.error())));
});
