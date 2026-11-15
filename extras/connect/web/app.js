import {initCodex} from '/codex.js';
const $ = id => document.getElementById(id);
let authenticated = false, muted = false, rfb = null, polling = null;
let volumeDragging = false;
let volumeSocket = null, volumeTimer = null, volumeReconnect = null, pendingVolume = null, sendingVolume = false, lastVolume = null;
let currentPath = '', activeUpload = null, systemPolling = null, confirmation = null, systemBusy = false, updateRunning = false;
const message = text => { $('message').textContent = text; };
const codexUI = initCodex(api, message);
let reconnectTimer = null, mediaTimer = null, selectedPlayer = null, currentPlayers = [];

async function api(path, data) {
  let response;
  try { response = await fetch('/api/' + path, data === undefined ? {signal: path==='status'?AbortSignal.timeout(4000):undefined} : {
    method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(data)
  }); } catch { waitForComputer(); throw new Error('Aguardando o computador. Reconectando automaticamente…'); }
  if (!response.ok) {
    if (response.status === 401 && path !== 'login') setConnected(false);
    const error = new Error(await response.text()); error.status = response.status; throw error;
  }
  return response.json();
}
function leaveImmersive(exitNative=true) {
  $('screen-shell').classList.remove('immersive','keyboard-open','controls-hidden'); document.body.classList.remove('screen-open');
  $('screen-keyboard').setAttribute('aria-pressed','false');
  if(exitNative && document.fullscreenElement===$('screen-shell')) document.exitFullscreen().catch(()=>{});
}
function enterImmersive() {
  $('screen-shell').classList.add('immersive');document.body.classList.add('screen-open');
  if(document.fullscreenEnabled && !$('screen-shell').matches(':fullscreen')) $('screen-shell').requestFullscreen({navigationUI:'hide'}).catch(()=>{});
}
$('screen-exit').onclick=()=>leaveImmersive();
$('screen-hide').onclick=()=>{$('screen-shell').classList.add('controls-hidden');$('screen-shell').classList.remove('keyboard-open');$('text').blur();$('screen-keyboard').setAttribute('aria-pressed','false');};
$('screen-keyboard').onclick=()=>{const visible=$('screen-shell').classList.toggle('keyboard-open');$('screen-keyboard').setAttribute('aria-pressed',String(visible));if(visible)$('text').focus();};
document.addEventListener('fullscreenchange',()=>{if(!document.fullscreenElement)leaveImmersive(false);});
function disconnectScreen() {
  stopPointer();
  leaveImmersive();
  if (rfb) {
    if ($('super').getAttribute('aria-pressed') === 'true') rfb.sendKey(0xffeb, 'MetaLeft', false);
    rfb.disconnect(); rfb = null;
  }
  $('super').setAttribute('aria-pressed', 'false');
  $('screen-placeholder').hidden = false;
  $('screen-toggle').textContent = 'Abrir tela';
  $('pan').setAttribute('aria-pressed', 'false');
  $('screen-touchpad').hidden=false;
}
function closeTerminal() {
  $('terminal-container').replaceChildren();
  $('terminal-toggle').textContent = 'Abrir terminal';
}
function setConnected(value) {
  authenticated = value;
  clearTimeout(reconnectTimer); reconnectTimer=null; $('connecting').hidden=true;
  $('pair').hidden = value; $('controls').hidden = !value;
  $('connection').textContent = value ? 'Conectado ao JLinux' : 'Não conectado';
  $('connection').classList.toggle('connected', value);
  clearInterval(polling);
  if (value) { startVolumeStream(); startMedia(); polling = setInterval(() => { if (!document.hidden) refresh().catch(e => message(e.message)); }, 15000); }
  else {
    clearTimeout(volumeReconnect); clearTimeout(volumeTimer); clearInterval(systemPolling); clearInterval(mediaTimer); codexUI.close();
    const socket = volumeSocket; volumeSocket = null; if (socket) socket.close();
    if (activeUpload) activeUpload.abort();
    pendingVolume = null; sendingVolume = false; lastVolume = null;
    disconnectScreen(); closeTerminal();
  }
}
async function refresh() {
  const status = await api('status');
  clearTimeout(reconnectTimer); reconnectTimer=null; $('connecting').hidden=true;
  if (!authenticated) setConnected(true);
  $('connection').textContent='Conectado ao JLinux';$('connection').classList.add('connected');startVolumeStream();
  $('volume').disabled = !status.volume; $('mute').disabled = !status.volume;
  if (status.volume) {
    if (!volumeDragging && pendingVolume === null && !sendingVolume && lastVolume === null) {
      $('volume').value = Math.round(status.volume.level * 100);
      $('volume-number').textContent = Math.round(status.volume.level * 100) + '%';
    }
    muted = status.volume.muted;
    $('mute').textContent = muted ? 'Ativar som' : 'Silenciar';
  } else $('volume-number').textContent = '—';
}
$('login').addEventListener('submit', async event => {
  event.preventDefault(); const button = event.submitter; button.disabled = true;
  try { await api('login', {key: $('key').value.trim(), remember: $('remember').checked}); $('key').value = ''; message(''); await refresh(); }
  catch (e) { message(e.message); } finally { button.disabled = false; }
});
function startVolumeStream() {
  if (!authenticated || volumeSocket && volumeSocket.readyState < 2) return;
  const socket = new WebSocket(`${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws/volume`);
  volumeSocket = socket;
  socket.onopen = () => { if (pendingVolume !== null) flushVolume(); };
  socket.onmessage = event => {
    const data = JSON.parse(event.data);
    if (data.error) { lastVolume = null; message(data.error); }
    else if (data.level === lastVolume) lastVolume = null;
  };
  socket.onclose = () => {
    if (volumeSocket !== socket) return;
    volumeSocket = null; lastVolume = null;
    if (authenticated) volumeReconnect = setTimeout(startVolumeStream, 2000);
  };
}
async function flushVolume() {
  clearTimeout(volumeTimer); volumeTimer = null;
  if (pendingVolume === null || sendingVolume) return;
  const level = pendingVolume; pendingVolume = null;
  if (volumeSocket?.readyState === WebSocket.OPEN && volumeSocket.bufferedAmount < 4096) {
    lastVolume = level; volumeSocket.send(JSON.stringify({level})); return;
  }
  sendingVolume = true;
  try { await api('volume', {level}); }
  catch (e) { message(e.message); }
  finally { sendingVolume = false; if (pendingVolume !== null) volumeTimer = setTimeout(flushVolume, 40); }
}
function moveVolume(final = false) {
  $('volume-number').textContent = $('volume').value + '%';
  pendingVolume = Number($('volume').value) / 100;
  if (final) flushVolume();
  else if (!volumeTimer) volumeTimer = setTimeout(flushVolume, 40);
}
$('volume').addEventListener('pointerdown', () => { volumeDragging = true; });
$('volume').addEventListener('pointerup', () => { volumeDragging = false; });
$('volume').addEventListener('pointercancel', () => { volumeDragging = false; moveVolume(true); });
$('volume').addEventListener('input', () => moveVolume());
$('volume').addEventListener('change', () => moveVolume(true));
$('mute').addEventListener('click', async () => {
  try { await api('volume', {mute: !muted}); await refresh(); message(''); }
  catch (e) { message(e.message); }
});
$('logout').addEventListener('click', async () => {
  try { await api('logout', {}); setConnected(false); message('Este iPhone foi desconectado.'); }
  catch (e) { message(e.message); }
});
document.querySelectorAll('[data-tab]').forEach(button => button.addEventListener('click', () => {
  document.querySelectorAll('[data-tab]').forEach(tab => tab.removeAttribute('aria-current'));
  button.setAttribute('aria-current', 'page');
  for (const name of ['volume', 'desktop', 'codex', 'terminal', 'files', 'system']) $(name + '-panel').hidden = name !== button.dataset.tab;
  if (button.dataset.tab !== 'desktop') disconnectScreen();
  clearInterval(systemPolling); clearInterval(mediaTimer); codexUI.close();
  message('');
  if (button.dataset.tab==='codex') codexUI.open();
  if (button.dataset.tab==='volume') startMedia();
  if (button.dataset.tab === 'files') loadFiles().catch(e => message(e.message));
  if (button.dataset.tab === 'system') {
    refreshSystem().catch(e => message(e.message));
    systemPolling = setInterval(() => { if (!document.hidden) refreshSystem().catch(e => message(e.message)); }, 5000);
  }
}));
$('screen-toggle').addEventListener('click', async () => {
  if (rfb) { disconnectScreen(); return; }
  $('screen-toggle').disabled = true;
  enterImmersive();
  try {
    const {default: RFB} = await import('/vendor/novnc/core/rfb.js');
    $('screen-placeholder').hidden = true;
    const connection = new RFB($('screen'), `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws/desktop`);
    rfb = connection; rfb.scaleViewport = true; rfb.resizeSession = false; rfb.background = '#000';
    applyScreenQuality(); rfb.showDotCursor = false;
    connection.addEventListener('connect', () => { startPointer(); $('screen-toggle').textContent = 'Fechar tela'; message('Tela conectada.'); });
    connection.addEventListener('disconnect', event => {
      if (rfb === connection) { disconnectScreen(); message(event.detail.clean ? 'Tela desconectada.' : 'Não foi possível abrir a tela. Entre novamente na sessão do JLinux e tente outra vez.'); }
    });
    connection.addEventListener('securityfailure', () => message('O acesso à tela foi recusado.'));
  } catch (e) { disconnectScreen(); message(e.message); }
  finally { $('screen-toggle').disabled = false; }
});
function applyScreenQuality() {
  if(!rfb)return;
  const sharp=$('screen-quality').value==='sharp';
  rfb.qualityLevel=sharp?9:5;rfb.compressionLevel=sharp?1:6;
}
$('screen-quality').onchange=applyScreenQuality;
const expand=document.createElement('button');expand.id='screen-expand';expand.type='button';expand.className='secondary';expand.textContent='Tela cheia';expand.onclick=enterImmersive;$('screen-shell').querySelector('.screen-toolbar').prepend(expand);
let pointerSocket=null, pointerRetry=null, pointerFrame=null, pointerHold=null;
let contact=null, dragging=false, suppressTap=false, dx=0, dy=0;
const contacts=new Set();
function sendPointer(data) {
  if(pointerSocket?.readyState===WebSocket.OPEN) pointerSocket.send(JSON.stringify(data));
}
function resetPointer() {
  clearTimeout(pointerHold);cancelAnimationFrame(pointerFrame);pointerFrame=null;dx=dy=0;
  if(dragging)sendPointer({button:false});
  dragging=false;contact=null;contacts.clear();
}
function stopPointer() {
  $('screen-touchpad').dataset.ready='false';
  resetPointer();clearTimeout(pointerRetry);
  if(pointerSocket){const socket=pointerSocket;pointerSocket=null;socket.close();}
}
function startPointer() {
  stopPointer();
  if(!rfb || document.hidden || !matchMedia('(pointer:coarse)').matches)return;
  const socket=new WebSocket(`${location.protocol==='https:'?'wss:':'ws:'}//${location.host}/ws/pointer`);
  pointerSocket=socket;
  socket.onopen=()=>{pad.dataset.ready='true';};
  socket.onmessage=event=>{const data=JSON.parse(event.data);if(data.error)message(data.error);};
  socket.onclose=()=>{if(pointerSocket!==socket)return;resetPointer();pointerSocket=null;pad.dataset.ready='false';pointerRetry=setTimeout(startPointer,1000);};
}
function flushPointer() {
  pointerFrame=null;
  if(dx||dy)sendPointer({dx:Math.max(-4096,Math.min(4096,dx)),dy:Math.max(-4096,Math.min(4096,dy))});
  dx=dy=0;
}
const pad=$('screen-touchpad');
pad.onpointerdown=event=>{
  event.preventDefault();pad.setPointerCapture(event.pointerId);contacts.add(event.pointerId);
  if(contacts.size>1){cancelAnimationFrame(pointerFrame);pointerFrame=null;dx=dy=0;clearTimeout(pointerHold);if(dragging)sendPointer({button:false});dragging=false;contact=null;suppressTap=true;$('screen-shell').classList.remove('controls-hidden');return;}
  suppressTap=false;
  const canvas=$('screen').querySelector('canvas');
  contact={id:event.pointerId,x:event.clientX,y:event.clientY,distance:0,scale:canvas?canvas.width/canvas.getBoundingClientRect().width:1};
  pointerHold=setTimeout(()=>{if(contact && contact.distance<8 && !suppressTap){dragging=true;sendPointer({button:true});}},350);
};
pad.onpointermove=event=>{
  if(!contact || contact.id!==event.pointerId)return;
  event.preventDefault();
  const x=event.clientX-contact.x,y=event.clientY-contact.y;
  contact.distance+=Math.hypot(x,y);contact.x=event.clientX;contact.y=event.clientY;
  if(contact.distance>8)clearTimeout(pointerHold);
  dx+=x*contact.scale;dy+=y*contact.scale;
  if(pointerFrame===null)pointerFrame=requestAnimationFrame(flushPointer);
};
function endPointer(event) {
  event.preventDefault();contacts.delete(event.pointerId);
  if(contact?.id!==event.pointerId)return;
  clearTimeout(pointerHold);cancelAnimationFrame(pointerFrame);flushPointer();
  if(dragging)sendPointer({button:false});
  else if(event.type==='pointerup' && !suppressTap && contact.distance<8){sendPointer({button:true});sendPointer({button:false});}
  dragging=false;contact=null;
}
pad.onpointerup=endPointer;pad.onpointercancel=endPointer;pad.onlostpointercapture=endPointer;
document.addEventListener('visibilitychange',()=>{if(document.hidden)stopPointer();else if(rfb)startPointer();});
window.addEventListener('blur',resetPointer);
document.addEventListener('keydown',event=>{if(event.key==='Escape')$('screen-shell').classList.remove('controls-hidden');});
$('pan').addEventListener('click', () => {
  if (!rfb) return;
  const enabled = $('pan').getAttribute('aria-pressed') !== 'true';
  $('screen-touchpad').hidden=enabled;
  rfb.scaleViewport = !enabled; rfb.clipViewport = enabled; rfb.dragViewport = enabled;
  $('pan').setAttribute('aria-pressed', String(enabled));
});
for (const [id, keysym, code] of [['enter', 0xff0d, 'Enter'], ['escape', 0xff1b, 'Escape']]) {
  $(id).addEventListener('click', () => { if (rfb) rfb.sendKey(keysym, code); });
}
$('super').addEventListener('click', () => {
  if (!rfb) return;
  const pressed = $('super').getAttribute('aria-pressed') !== 'true';
  rfb.sendKey(0xffeb, 'MetaLeft', pressed);
  $('super').setAttribute('aria-pressed', String(pressed));
});
$('typing').addEventListener('submit', event => {
  event.preventDefault();
  if (!rfb) { message('Abra a tela antes de enviar texto.'); return; }
  for (const character of $('text').value) {
    const codepoint = character.codePointAt(0);
    rfb.sendKey(codepoint > 255 ? 0x01000000 + codepoint : codepoint);
  }
  $('text').value = ''; message('Texto enviado para a janela selecionada.');
});
$('terminal-toggle').addEventListener('click', () => {
  if ($('terminal-container').children.length) { closeTerminal(); return; }
  const frame = document.createElement('iframe');
  frame.title = 'Terminal do JLinux'; frame.src = '/terminal/';
  $('terminal-container').append(frame); $('terminal-toggle').textContent = 'Fechar terminal';
});
function formatSize(size) {
  if (size < 1024) return size + ' B';
  const units = ['KB', 'MB', 'GB', 'TB']; let index = -1;
  do { size /= 1024; index++; } while (size >= 1024 && index < units.length - 1);
  return size.toFixed(size < 10 ? 1 : 0) + ' ' + units[index];
}
async function loadFiles(path = currentPath) {
  const query = new URLSearchParams({path, hidden: $('files-hidden').checked ? '1' : '0'});
  const data = await api('files?' + query);
  currentPath = data.path; $('file-path').textContent = currentPath ? 'Seus arquivos / ' + currentPath : 'Seus arquivos';
  $('files-back').disabled = !currentPath;
  $('file-list').replaceChildren();
  for (const item of data.entries) {
    const row = document.createElement('div'); row.className = 'file-entry';
    const symbol = document.createElement('span'); symbol.className = 'file-symbol'; symbol.textContent = item.directory ? '▱' : '▤'; symbol.setAttribute('aria-hidden', 'true');
    const info = document.createElement('div'), title = document.createElement('strong'), detail = document.createElement('small');
    title.textContent = item.name; detail.textContent = item.directory ? 'Pasta' : formatSize(item.size);
    info.append(title, detail);
    const destination = [currentPath, item.name].filter(Boolean).join('/');
    let action;
    if (item.directory) {
      action = document.createElement('button'); action.textContent = 'Abrir'; action.setAttribute('aria-label', 'Abrir ' + item.name);
      action.onclick = () => loadFiles(destination).catch(e => message(e.message));
    } else {
      action = document.createElement('a'); action.textContent = 'Baixar'; action.download = item.name;
      action.href = '/api/files/download?' + new URLSearchParams({path: destination}); action.setAttribute('aria-label', 'Baixar ' + item.name);
    }
    row.append(symbol, info, action); $('file-list').append(row);
  }
  if (!data.entries.length || data.truncated) {
    const notice = document.createElement('p'); notice.className = 'empty-folder';
    notice.textContent = data.truncated ? 'Exibindo os primeiros 1000 itens desta pasta.' : 'Esta pasta está vazia.';
    $('file-list').append(notice);
  }
}
$('files-back').onclick = () => loadFiles(currentPath.split('/').slice(0, -1).join('/')).catch(e => message(e.message));
$('files-refresh').onclick = () => loadFiles().catch(e => message(e.message));
$('files-hidden').onchange = () => loadFiles().catch(e => message(e.message));
$('upload-cancel').onclick = () => { if (activeUpload) activeUpload.abort(); };
$('file-input').onchange = async () => {
  const files = Array.from($('file-input').files), destination = currentPath;
  if (!files.length || activeUpload) return;
  $('file-input').disabled = true; $('upload-cancel').hidden = false; $('upload-progress').hidden = false;
  try {
    for (const [index, file] of files.entries()) {
      if (file.size > 2 * 1024 ** 3) throw new Error('O limite de envio é de 2 GB por arquivo.');
      $('upload-status').textContent = `Enviando ${file.name} (${index + 1}/${files.length})…`;
      await new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest(); activeUpload = xhr;
        xhr.open('POST', '/api/files/upload?' + new URLSearchParams({path: destination, name: file.name}));
        xhr.setRequestHeader('Content-Type', 'application/octet-stream');
        xhr.upload.onprogress = event => { if (event.lengthComputable) $('upload-progress').value = event.loaded / event.total * 100; };
        xhr.onload = () => xhr.status === 201 ? resolve() : reject(new Error(xhr.responseText || 'O envio falhou.'));
        xhr.onerror = () => reject(new Error('A conexão foi interrompida. Verifique a pasta antes de enviar novamente.'));
        xhr.onabort = () => reject(new Error('Envio cancelado. Os arquivos já concluídos foram mantidos.'));
        xhr.send(file);
      });
    }
    $('upload-status').textContent = 'Arquivos enviados para o JLinux.';
  } catch (e) { $('upload-status').textContent = e.message; }
  finally {
    activeUpload = null; $('file-input').disabled = false; $('file-input').value = '';
    $('upload-cancel').hidden = true; $('upload-progress').hidden = true;
    loadFiles().catch(e => message(e.message));
  }
};
function systemButtons() {
  document.querySelectorAll('[data-action]').forEach(button => {
    button.disabled = systemBusy || updateRunning && ['reboot', 'poweroff', 'update'].includes(button.dataset.action);
  });
}
async function refreshSystem() {
  const status = await api('system/status'); updateRunning = status.running;
  const labels = {idle: 'Pronto para atualizar', running: 'Atualizando…', success: 'Atualização concluída', failed: 'A atualização encontrou um problema'};
  $('update-state').textContent = labels[status.state] || 'Status indisponível';
  $('update-log').textContent = status.log; $('update-log').hidden = !status.log;
  systemButtons();
}
$('system-refresh').onclick = () => refreshSystem().catch(e => message(e.message));
document.querySelectorAll('[data-action]').forEach(button => button.onclick = async () => {
  if (systemBusy) return;
  systemBusy = true; systemButtons();
  try {
    const action = button.dataset.action;
    const prepared = await api('system/prepare', {action});
    confirmation = {action, confirmation: prepared.confirmation};
    $('confirm-title').textContent = prepared.title; $('confirm-explanation').textContent = prepared.explanation;
    $('system-confirm').showModal();
  } catch (e) { message(e.message); }
  finally { systemBusy = false; systemButtons(); }
});
$('system-confirm').addEventListener('close', () => { confirmation = null; });
$('confirm-action').onclick = async () => {
  if (!confirmation || systemBusy) return;
  const action = confirmation; confirmation = null; $('system-confirm').close(); systemBusy = true; systemButtons();
  try {
    await api('system/action', action);
    message(action.action === 'update' ? 'Atualização iniciada. Você pode acompanhar o progresso aqui.' : 'Ação confirmada. A conexão poderá ser interrompida.');
    await refreshSystem();
  } catch (e) { message(e.message); }
  finally { systemBusy = false; systemButtons(); }
};

function waitForComputer() {
  $('connection').textContent='Reconectando…';$('connection').classList.remove('connected');
  if(!authenticated){$('pair').hidden=true;$('connecting').hidden=false;}
  if(!reconnectTimer)reconnectTimer=setTimeout(()=>{reconnectTimer=null;reconnect();},5000);
}
async function reconnect(){
  clearTimeout(reconnectTimer);reconnectTimer=null;
  try{await refresh();message('');}catch(error){if(error.status===401)setConnected(false);else waitForComputer();}
}
$('reconnect').onclick=reconnect;
window.addEventListener('online',reconnect);
if('serviceWorker' in navigator) navigator.serviceWorker.register('/sw.js').catch(()=>{});
async function refreshMedia() {
  const data=await api('media');currentPlayers=data.players;
  if(!currentPlayers.some(player=>player.id===selectedPlayer))selectedPlayer=currentPlayers[0]?.id||null;
  const player=currentPlayers.find(item=>item.id===selectedPlayer);
  $('media-title').textContent=player?.title||'Nenhum player aberto';$('media-artist').textContent=player?.artist||'Abra uma música no computador.';
  const select=$('media-player');
  if(JSON.stringify([...select.options].map(option=>[option.value,option.textContent]))!==JSON.stringify(currentPlayers.map(item=>[item.id,item.name]))){
    select.replaceChildren();for(const item of currentPlayers){const option=document.createElement('option');option.value=item.id;option.textContent=item.name;select.append(option);}
  }
  select.value=selectedPlayer||'';select.hidden=currentPlayers.length<2;
  $('media-back').disabled=!player?.can_seek;$('media-forward').disabled=!player?.can_seek;
  $('media-previous').disabled=!player?.can_previous;$('media-next').disabled=!player?.can_next;$('media-play').disabled=!player?.can_play_pause;
  $('media-play').textContent=player?.playing?'Ⅱ':'▶';$('media-play').setAttribute('aria-label',player?.playing?'Pausar':'Reproduzir');
}
function startMedia(){clearInterval(mediaTimer);refreshMedia().catch(error=>message(error.message));mediaTimer=setInterval(()=>{if(!document.hidden)refreshMedia().catch(error=>message(error.message));},2500);}
$('media-player').onchange=()=>{selectedPlayer=$('media-player').value;refreshMedia().catch(error=>message(error.message));};
for(const [id,action] of [['media-previous','previous'],['media-play','play_pause'],['media-next','next'],['media-back','seek_back'],['media-forward','seek_forward']]){
  $(id).onclick=async()=>{const button=$(id);button.disabled=true;try{await api('media/action',{player:selectedPlayer,action});await refreshMedia();}catch(error){message(error.message);button.disabled=false;}};
}
// The pairing key stays out of server logs, browser history and persistent web storage.
const fragment = location.hash.slice(1);
history.replaceState(null, '', location.pathname);
if (fragment) {
  try { await api('login', {key: fragment, remember: true}); await refresh(); }
  catch (e) { if(e.status===401)setConnected(false);message(e.message); }
} else {
  await reconnect();
}
