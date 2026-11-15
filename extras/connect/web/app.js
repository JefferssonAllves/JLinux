const $ = id => document.getElementById(id);
let authenticated = false, muted = false, rfb = null, polling = null;
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
  if (value) polling = setInterval(() => { if (!document.hidden) refresh().catch(e => message(e.message)); }, 15000);
  else { disconnectScreen(); closeTerminal(); }
}
async function refresh() {
  const status = await api('status');
  if (!authenticated) setConnected(true);
  $('volume').disabled = !status.volume; $('mute').disabled = !status.volume;
  if (status.volume) {
    if (document.activeElement !== $('volume')) $('volume').value = Math.round(status.volume.level * 100);
    $('volume-number').textContent = Math.round(status.volume.level * 100) + '%';
    muted = status.volume.muted;
    $('mute').textContent = muted ? 'Ativar som' : 'Silenciar';
  } else $('volume-number').textContent = '—';
}
$('login').addEventListener('submit', async event => {
  event.preventDefault(); const button = event.submitter; button.disabled = true;
  try { await api('login', {key: $('key').value.trim()}); $('key').value = ''; message(''); await refresh(); }
  catch (e) { message(e.message); } finally { button.disabled = false; }
});
$('volume').addEventListener('input', () => $('volume-number').textContent = $('volume').value + '%');
$('volume').addEventListener('change', async () => {
  try { await api('volume', {level: Number($('volume').value) / 100}); await refresh(); message(''); }
  catch (e) { message(e.message); }
});
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
  for (const name of ['volume', 'desktop', 'terminal']) $(name + '-panel').hidden = name !== button.dataset.tab;
  if (button.dataset.tab !== 'desktop') disconnectScreen();
  message('');
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
// The pairing key stays out of server logs, browser history and persistent web storage.
const fragment = location.hash.slice(1);
history.replaceState(null, '', location.pathname);
if (fragment) {
  try { await api('login', {key: fragment}); await refresh(); }
  catch (e) { message(e.message); }
} else {
  try { await refresh(); } catch { setConnected(false); }
}
