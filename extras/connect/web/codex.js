// Only human prompts, answer text and explicit decisions are rendered here.
export function initCodex(api, message) {
  const $ = id => document.getElementById(id);
  const labels = {working: 'Trabalhando', waiting: 'Aguardando você', idle: 'Parado', completed: 'Finalizado · não lido', error: 'Erro'};
  let seen = {};
  try {const saved = JSON.parse(localStorage.getItem('jlinux-codex-seen') || '{}'); if (saved && typeof saved === 'object' && !Array.isArray(saved)) seen = saved;} catch {}
  const displayState = agent => agent.state === 'idle' && agent.completion && seen[agent.id] !== agent.completion ? 'completed' : agent.state;
  function markRead(agent) {
    if (!opened || document.hidden || !agent?.completion) return;
    seen[agent.id] = agent.completion;
    try {localStorage.setItem('jlinux-codex-seen', JSON.stringify(seen));} catch {}
  }
  let currentAgent = null;
  let selected = null, agents = [], timer = null, busy = false, cursor = null, historyCursor = null, etag = null, fingerprint = '', generation = 0, displayed = [], historyLoaded = false;
  let stream = null, reconnect = null, opened = false;
  function stopStream() {
    clearTimeout(reconnect); reconnect = null;
    if (stream) {const previous = stream; stream = null; previous.close();}
  }
  function startStream() {
    stopStream();
    if (!opened || !selected || document.hidden) return;
    const ident = selected;
    const socket = new WebSocket(`${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/ws/codex/${ident}`);
    stream = socket;
    socket.onmessage = event => {
      if (stream !== socket || selected !== ident) return;
      renderMessages(JSON.parse(event.data).messages);
    };
    socket.onclose = () => {
      if (stream !== socket) return;
      stream = null;
      reconnect = setTimeout(startStream, 1000);
    };
  }
  document.addEventListener('visibilitychange', () => {if (document.hidden) stopStream(); else startStream();});
  const status = (node, state) => {node.className = 'agent-status ' + state; node.textContent = labels[state] || 'Indisponível';};
  function renderList() {
    $('agent-list').replaceChildren();
    const filter = $('agent-filter').value;
    const visible = agents.map(agent => ({...agent, state: displayState(agent)})).filter(agent => filter === 'all' || agent.state === filter);
    visible.sort((a, b) => ({waiting:0,completed:1,working:2,error:3,idle:4}[a.state] - {waiting:0,completed:1,working:2,error:3,idle:4}[b.state]) || b.updated-a.updated);
    const groups = new Map();
    for (const agent of visible) {
      const path = agent.cwd || '';
      if (!groups.has(path)) groups.set(path, []);
      groups.get(path).push(agent);
    }
    for (const [path, members] of [...groups].sort(([a], [b]) => a.localeCompare(b, 'pt-BR'))) {
      const section = document.createElement('section'); section.className = 'agent-group';
      const heading = document.createElement('h3'), location = document.createElement('p');
      heading.textContent = `${path.split('/').filter(Boolean).pop() || (path ? '/' : 'Sem pasta associada')} · ${members.length}`;
      location.textContent = path; location.className = 'agent-group-path';
      section.append(heading, location);
      for (const agent of members) {
        const button = document.createElement('button'); button.className = 'agent-card'; button.type = 'button';
        const content = document.createElement('div'), title = document.createElement('strong'), subtitle = document.createElement('small'), badge = document.createElement('span');
        title.textContent = agent.title; subtitle.textContent = (agent.parent ? 'Agente auxiliar · ' : '') + (agent.role || agent.cwd || 'Codex');
        status(badge, agent.state); content.append(title, subtitle); button.append(content, badge);
        button.onclick = () => select(agent.id).catch(error => message(error.message)); section.append(button);
      }
      $('agent-list').append(section);
    }
    $('agents-empty').hidden = visible.length > 0;
    $('agents-more').hidden = !cursor;
    $('agents-count').textContent = `${agents.filter(a=>a.state==='working').length} trabalhando · ${agents.filter(a=>a.state==='waiting').length} aguardando · ${agents.filter(a=>displayState(a)==='completed').length} finalizados não lidos`;
  }
  async function listing(more = false) {
    const query = new URLSearchParams({search: $('agent-search').value});
    if (more && cursor) query.set('cursor', cursor);
    const data = await api('codex/threads?' + query);
    agents = more ? [...new Map([...agents,...data.agents].map(agent=>[agent.id,agent])).values()] : data.agents;
    cursor = data.cursor; renderList();
  }
  function renderRequests(requests) {
    const key = JSON.stringify(requests);
    if ($('agent-requests').dataset.content === key) return;
    $('agent-requests').dataset.content = key; $('agent-requests').replaceChildren();
    for (const request of requests) {
      const card = document.createElement('form'); card.className = 'agent-request';
      const heading = document.createElement('strong'); heading.textContent = request.kind === 'question' ? 'Sua resposta é necessária' : request.title;
      card.append(heading);
      const fields = new Map();
      if (request.kind === 'question') {
        for (const question of request.questions) {
          const label = document.createElement('label'); label.textContent = question.title;
          const input = document.createElement('input'); input.type = question.secret ? 'password' : 'text'; input.required = true; input.autocomplete = 'off';
          input.setAttribute('aria-label', question.title); label.append(input); card.append(label); fields.set(question.id, input);
          const choices = document.createElement('div'); choices.className = 'question-options';
          for (const option of question.options) {
            const button = document.createElement('button'); button.type='button'; button.className='secondary'; button.textContent=option;
            button.onclick=()=>{input.value=option;}; choices.append(button);
          }
          card.append(choices);
        }
        const submit=document.createElement('button'); submit.className='primary'; submit.textContent='Enviar resposta'; card.append(submit);
      } else if (request.kind === 'approval') {
        const reason=document.createElement('p'); reason.textContent=request.reason; card.append(reason);
        if(request.details){const detail=document.createElement('pre');detail.textContent=request.details;card.append(detail);}
        for(const [decision,label] of [['accept','Autorizar uma vez'],['decline','Recusar']]) {
          const button=document.createElement('button');button.type='button';button.className='secondary';button.textContent=label;
          button.onclick=()=>reply({decision});card.append(button);
        }
      }
      async function reply(answer) {
        const ident=selected;
        card.querySelectorAll('button').forEach(button=>button.disabled=true);
        try {await api(`codex/threads/${ident}/reply`, {request:request.id,...answer}); await detail();}
        catch(error){message(error.message);card.querySelectorAll('button').forEach(button=>button.disabled=false);}
      }
      card.onsubmit=event=>{event.preventDefault();reply({answers:Object.fromEntries([...fields].map(([id,input])=>[id,input.value]))});};
      $('agent-requests').append(card);
    }
  }
  function renderMessages(items, prepend=false) {
    const merged=[...new Map((prepend?[...items,...displayed]:[...displayed,...items]).map(item=>[item.id,item])).values()];
    const key=JSON.stringify(merged);
    if(key===fingerprint)return;
    const empty=displayed.length===0, oldHeight=$('agent-chat').scrollHeight, oldTop=$('agent-chat').scrollTop;
    displayed=merged;
    const nearBottom=$('agent-chat').scrollHeight-$('agent-chat').scrollTop-$('agent-chat').clientHeight<100;
    $('agent-chat').replaceChildren();fingerprint=key;
    const fragment=document.createDocumentFragment();
    for(const item of merged){
      const bubble=document.createElement('article');bubble.className='agent-message '+item.role;
      const label=document.createElement('small');label.textContent=item.role==='user'?'Você':'Codex';
      const text=document.createElement('div');text.textContent=item.text;bubble.append(label,text);fragment.append(bubble);
    }
    $('agent-chat').append(fragment);
    $('agent-chat').scrollTop=prepend?oldTop+$('agent-chat').scrollHeight-oldHeight:nearBottom||empty?$('agent-chat').scrollHeight:oldTop;
    $('agent-chat-empty').hidden=$('agent-chat').children.length>0;
  }
  async function detail(older=false) {
    if(!selected)return;
    const ident=selected, version=generation;
    const query=older&&historyCursor?'?'+new URLSearchParams({cursor:historyCursor}):'';
    const response=await fetch(`/api/codex/threads/${ident}${query}`, {headers:!older&&etag?{'If-None-Match':etag}:{}});
    if(response.status===304){if(selected===ident && generation===version)markRead(currentAgent);return;}
    if(!response.ok)throw new Error(await response.text());
    const data=await response.json();if(selected!==ident || generation!==version)return;
    if(!older)etag=response.headers.get('ETag');
    if(older||!historyLoaded){historyCursor=data.cursor;historyLoaded=true;} $('agent-history').hidden=!historyCursor;
    $('agent-title').textContent=data.agent.title; status($('agent-state'),data.agent.state);
    $('agent-project').textContent=data.agent.cwd;
    $('agent-parent').hidden=!data.agent.parent; $('agent-parent').onclick=()=>select(data.agent.parent).catch(error=>message(error.message));
    $('agent-prompt').disabled=data.agent.can_send===false; $('agent-send').disabled=data.agent.can_send===false;
    $('agent-prompt').placeholder=data.agent.can_send===false?'Envie sua orientação ao agente principal':'Escreva seu prompt…';
    renderMessages(data.messages,older);renderRequests(data.requests);
    if (!older) {currentAgent=data.agent;markRead(currentAgent);}
  }
  async function select(ident) {
    stopStream();currentAgent=null;generation++;selected=ident;etag=null;fingerprint='';historyCursor=null;historyLoaded=false;displayed=[];
    $('agents-overview').hidden=true;$('agent-conversation').hidden=false;$('agent-chat').replaceChildren();$('agent-requests').replaceChildren();$('agent-requests').dataset.content='';
    $('agent-title').textContent='Carregando agente…';
    await detail();startStream();
  }
  $('agents-back').onclick=()=>{stopStream();generation++;selected=null;$('agents-overview').hidden=false;$('agent-conversation').hidden=true;listing().catch(error=>message(error.message));};
  $('agent-history').onclick=()=>detail(true).catch(error=>message(error.message));
  $('agent-filter').onchange=renderList;
  let searchTimer;
  $('agent-search').oninput=()=>{clearTimeout(searchTimer);searchTimer=setTimeout(()=>listing().catch(error=>message(error.message)),250);};
  $('agent-compose').onsubmit=async event=>{
    event.preventDefault();if(!selected || !$('agent-prompt').value.trim())return;
    const input=$('agent-prompt'), button=$('agent-send'), ident=selected, text=input.value;
    button.disabled=true;
    try{await api(`codex/threads/${ident}/prompt`,{text});if(selected===ident && input.value===text)input.value='';etag=null;await detail();}
    catch(error){message(error.message);}finally{button.disabled=input.disabled;}
  };
  $('agents-more').onclick=()=>listing(true).catch(error=>message(error.message));
  $('agent-new').onclick=()=>{$('agent-cwd').value=agents.find(a=>a.id===selected)?.cwd||agents[0]?.cwd||'/srv/JLinux';$('agent-new-dialog').showModal();};
  $('agent-create').onclick=async()=>{
    const button=$('agent-create');button.disabled=true;
    try{const agent=await api('codex/threads',{cwd:$('agent-cwd').value});$('agent-new-dialog').close();await select(agent.id);$('agent-prompt').focus();}
    catch(error){message(error.message);}finally{button.disabled=false;}
  };
  async function tick(){
    if(busy || document.hidden)return;busy=true;
    try{if(selected)await detail();else await listing();}catch(error){message(error.message);}finally{busy=false;}
  }
  return {open(){opened=true;clearInterval(timer);tick();startStream();timer=setInterval(tick,2000);},close(){opened=false;stopStream();clearInterval(timer);generation++;}};
}
