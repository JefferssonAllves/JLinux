const $ = id => document.getElementById(id);
let authenticated = false, muted = false, rfb = null, polling = null;
let volumeDragging = false;
let volumeSocket = null, volumeTimer = null, volumeReconnect = null, pendingVolume = null, sendingVolume = false, lastVolume = null;
let currentPath = '', activeUpload = null, systemPolling = null, confirmation = null, systemBusy = false, updateRunning = false;
const message = text => { $('message').textContent = text; };
async function api(path, data) {
  const response = await fetch('/api/' + path, data === undefined ? {} : {
    method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(data)
  });
  if (!response.ok) {
    if (response.status === 401 && path !== 'login') setConnected(false);
    throw new Error(await response.text());
  }
  return response.json();
}
function disconnectScreen() {
  if (rfb) {
    if ($('super').getAttribute('aria-pressed') === 'true') rfb.sendKey(0xffeb, 'MetaLeft', false);
    rfb.disconnect(); rfb = null;
  }
  $('super').setAttribute('aria-pressed', 'false');
  $('screen-placeholder').hidden = false;
  $('screen-toggle').textContent = 'Abrir tela';
  $('pan').setAttribute('aria-pressed', 'false');
}
function closeTerminal() {
  $('terminal-container').replaceChildren();
  $('terminal-toggle').textContent = 'Abrir terminal';
}
function setConnected(value) {
  authenticated = value;
  $('pair').hidden = value; $('controls').hidden = !value;
  $('connection').textContent = value ? 'Conectado ao JLinux' : 'Não conectado';
  $('connection').classList.toggle('connected', value);
  clearInterval(polling);
  if (value) { startVolumeStream(); polling = setInterval(() => { if (!document.hidden) refresh().catch(e => message(e.message)); }, 15000); }
  else {
    clearTimeout(volumeReconnect); clearTimeout(volumeTimer); clearInterval(systemPolling);
    const socket = volumeSocket; volumeSocket = null; if (socket) socket.close();
    if (activeUpload) activeUpload.abort();
    pendingVolume = null; sendingVolume = false; lastVolume = null;
    disconnectScreen(); closeTerminal();
  }
}
async function refresh() {
  const status = await api('status');
  if (!authenticated) setConnected(true);
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
  for (const name of ['volume', 'desktop', 'terminal', 'files', 'system']) $(name + '-panel').hidden = name !== button.dataset.tab;
  if (button.dataset.tab !== 'desktop') disconnectScreen();
  clearInterval(systemPolling);
  message('');
  if (button.dataset.tab === 'files') loadFiles().catch(e => message(e.message));
  if (button.dataset.tab === 'system') {
    refreshSystem().catch(e => message(e.message));
    systemPolling = setInterval(() => { if (!document.hidden) refreshSystem().catch(e => message(e.message)); }, 5000);
  }
}));
$('screen-toggle').addEventListener('click', async () => {
  if (rfb) { disconnectScreen(); return; }
  $('screen-toggle').disabled = true;
  try {
    const {default: RFB} = await import('/vendor/novnc/core/rfb.js');
    $('screen-placeholder').hidden = true;
    const connection = new RFB($('screen'), `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws/desktop`);
    rfb = connection; rfb.scaleViewport = true; rfb.resizeSession = false;
    rfb.qualityLevel = 5; rfb.compressionLevel = 6; rfb.showDotCursor = true;
    connection.addEventListener('connect', () => { $('screen-toggle').textContent = 'Fechar tela'; message('Tela conectada.'); });
    connection.addEventListener('disconnect', event => {
      if (rfb === connection) { disconnectScreen(); message(event.detail.clean ? 'Tela desconectada.' : 'Não foi possível abrir a tela. Entre novamente na sessão do JLinux e tente outra vez.'); }
    });
    connection.addEventListener('securityfailure', () => message('O acesso à tela foi recusado.'));
  } catch (e) { disconnectScreen(); message(e.message); }
  finally { $('screen-toggle').disabled = false; }
});
$('pan').addEventListener('click', () => {
  if (!rfb) return;
  const enabled = $('pan').getAttribute('aria-pressed') !== 'true';
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

// The pairing key stays out of server logs, browser history and persistent web storage.
const fragment = location.hash.slice(1);
history.replaceState(null, '', location.pathname);
if (fragment) {
  try { await api('login', {key: fragment, remember: true}); await refresh(); }
  catch (e) { message(e.message); }
} else {
  try { await refresh(); } catch { setConnected(false); }
}
