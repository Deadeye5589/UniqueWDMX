#pragma once
#include <Arduino.h>

static const char STYLE_CSS[] PROGMEM = R"css(
:root{--bg:#111;--card:#1e1e1e;--fg:#eee;--mut:#9a9a9a;--acc:#2d8cf0;--warn:#f5a623;--in:#2a2a2a}
*{box-sizing:border-box}
body{margin:0;font-family:Helvetica,Arial,sans-serif;background:var(--bg);color:var(--fg)}
header{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:8px;padding:10px 16px;background:#000}
header h1{font-size:1.2em;margin:0}
.devname{flex:1;text-align:center;font-weight:bold;color:var(--warn);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
nav{display:flex;flex-wrap:wrap;gap:4px}
nav a{color:var(--fg);text-decoration:none;padding:6px 10px;border-radius:6px}
nav a.act{background:var(--acc)}
main{max-width:640px;margin:auto;padding:8px 12px}
.card{background:var(--card);border-radius:12px;padding:14px 16px;margin:12px 0}
.card h2{margin:0 0 10px;font-size:.95em;color:var(--mut);font-weight:normal;text-transform:uppercase;letter-spacing:.06em}
.row{display:flex;align-items:center;gap:12px}
.ctl .val{font-size:1.8em;min-width:3.2em;text-align:right}
input[type=range]{flex:1;accent-color:var(--acc);height:36px;min-width:0}
input,select{background:var(--in);color:var(--fg);border:1px solid #444;border-radius:6px;padding:8px;font-size:1em}
input[type=number]{width:84px}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--acc);vertical-align:middle}
.full{width:100%}
label{display:block;margin:10px 0 4px;color:var(--mut);font-size:.9em}
label.inl{display:flex;align-items:center;gap:8px;color:var(--fg)}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}
.grid3{display:grid;grid-template-columns:2fr 1fr 1fr;gap:6px;align-items:center}
.presets{display:grid;grid-template-columns:repeat(auto-fit,minmax(100px,1fr));gap:8px}
.groups{display:flex;flex-wrap:wrap;gap:4px 14px}
.groups label{margin:4px 0}
button{background:var(--acc);color:#fff;border:0;border-radius:8px;padding:12px 16px;font-size:1em;cursor:pointer}
button:active{filter:brightness(.8)}
button.sec{background:#444}
button.dang{background:#c0392b}
.badge{padding:4px 10px;border-radius:12px;background:#444;font-size:.85em}
.badge.net{background:var(--warn);color:#000}
.banner{background:var(--warn);color:#000;padding:10px 14px;border-radius:8px;margin-top:12px}
.hint{color:var(--mut);font-size:.85em}
.msg{margin-top:10px;min-height:1.2em}
.hide{display:none}
#scan div{padding:8px;border-bottom:1px solid #333;cursor:pointer}
#scan div:hover{background:#333}
footer{text-align:center;color:var(--mut);font-size:.8em;padding:16px}
)css";

static const char COMMON_JS[] PROGMEM = R"js(
const $=id=>document.getElementById(id);
document.addEventListener('DOMContentLoaded',()=>{
  loadName();
  const a=document.querySelector(`nav a[href="${location.pathname}"]`);if(a)a.classList.add('act');
});
async function getJson(u){const r=await fetch(u);if(!r.ok)throw new Error(r.status);return r.json();}
async function loadName(){
  try{
    const n=(await getJson('/api/name')).name;
    $('devname').textContent=n;
    if(n)document.title=n+' | '+document.title.split(' | ').pop();
  }catch(_){}
}
function fill(form,o){
  for(const el of form.elements){
    if(!el.name||!(el.name in o))continue;
    if(el.type==='checkbox')el.checked=!!o[el.name];else el.value=o[el.name];
  }
}
async function submitForm(form,url,msg){
  if(!form.reportValidity())return false;
  const p=new URLSearchParams();
  for(const el of form.elements){
    if(!el.name)continue;
    p.append(el.name,el.type==='checkbox'?(el.checked?'1':'0'):el.value);
  }
  msg.textContent='Saving...';
  try{
    const r=await fetch(url,{method:'POST',body:p});
    msg.textContent=await r.text();
    return r.ok;
  }catch(e){msg.textContent='Error: '+e;return false;}
}
async function postCmd(url,question){
  if(question&&!confirm(question))return;
  try{const r=await fetch(url,{method:'POST'});alert(await r.text());}catch(e){alert('Error: '+e);}
}
)js";

#define PAGE_HEAD(title)                                                                       \
  "<!DOCTYPE html><html><head><meta charset='utf-8'>"                                          \
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"                        \
  "<title>UniqueWDMX - " title "</title>"                                                      \
  "<link rel='stylesheet' href='/style.css'><script src='/common.js'></script></head><body>"   \
  "<header><h1>UniqueWDMX</h1><span id='devname' class='devname'></span>"                       \
  "<nav><a href='/'>Control</a><a href='/wifi'>WiFi</a>"                                       \
  "<a href='/dmx'>DMX / ArtNet</a><a href='/system'>System</a></nav></header><main>"

static const char INDEX_HTML[] PROGMEM = PAGE_HEAD("Control") R"html(
<div class="row" style="justify-content:space-between;margin-top:12px">
  <span>Source: <span id="src" class="badge">-</span></span>
  <span id="conn" class="hint">connecting...</span>
</div>
<div id="banner" class="banner hide"></div>

<div class="card ctl">
  <h2>Fan Speed</h2>
  <div class="row">
    <input type="range" id="fanR" min="0" max="255" value="0">
    <input type="number" id="fanN" min="0" max="255" value="0">
  </div>
  <div class="row" style="justify-content:flex-end"><span class="val" id="fanP">0%</span></div>
</div>

<div class="card ctl">
  <h2>Pump</h2>
  <div class="row">
    <input type="range" id="pumpR" min="0" max="255" value="0">
    <input type="number" id="pumpN" min="0" max="255" value="0">
  </div>
  <div class="row" style="justify-content:flex-end"><span class="val" id="pumpP">0%</span></div>
</div>

<div class="card">
  <h2>Quick Select</h2>
  <div id="presets" class="presets"></div>
</div>

<div class="card">
  <h2>Sync</h2>
  <div class="row" style="justify-content:space-between">
    <button id="syncBtn" class="sec">Sync Master: Off</button>
    <span id="syncInfo" class="hint"></span>
  </div>
</div>
</main>
<footer id="foot"></footer>
<script>
const keys=['fan','pump'];
const ctl={};let ws,presetsKey='',pend={},timer=null;
for(const k of keys){
  const c={r:$(k+'R'),n:$(k+'N'),p:$(k+'P'),drag:false};ctl[k]=c;
  const pct=v=>c.p.textContent=Math.round(v/2.55)+'%';
  c.r.addEventListener('input',()=>{c.drag=true;c.n.value=c.r.value;pct(c.r.value);send({[k]:+c.r.value});});
  // 'change' does not fire if the slider ends on its start value, so also end the drag on release
  for(const ev of ['change','pointerup','pointercancel','touchend','mouseup','keyup','blur'])
    c.r.addEventListener(ev,()=>{c.drag=false;});
  c.n.addEventListener('change',()=>{
    const v=Math.max(0,Math.min(255,parseInt(c.n.value)||0));
    c.n.value=v;c.r.value=v;pct(v);send({[k]:v});
  });
  c.n.addEventListener('keydown',e=>{if(e.key==='Enter')c.n.blur();});
  c.pct=pct;
}
function send(o){
  Object.assign(pend,o);
  if(timer)return;
  timer=setTimeout(flush,40);
}
function flush(){
  clearTimeout(timer);timer=null;
  if(Object.keys(pend).length&&ws&&ws.readyState===1)ws.send(JSON.stringify(pend));
  pend={};
}
function applyPreset(i,p){
  // Drop queued slider values so they cannot override the preset on the server
  delete pend.fan;delete pend.pump;flush();
  const vals={fan:p.f,pump:p.p};
  for(const k of keys){const c=ctl[k];c.drag=false;c.r.value=vals[k];c.n.value=vals[k];c.pct(vals[k]);}
  if(ws&&ws.readyState===1)ws.send(JSON.stringify({preset:i}));
}
function renderPresets(list){
  const key=JSON.stringify(list);if(key===presetsKey)return;presetsKey=key;
  const box=$('presets');box.innerHTML='';
  list.forEach((p,i)=>{
    const b=document.createElement('button');
    b.textContent=p.n;b.title=`Fan ${p.f} / Pump ${p.p}`;
    b.onclick=()=>applyPreset(i,p);
    box.appendChild(b);
  });
}
const SRC={manual:'Manual',artnet:'ArtNet',sacn:'sACN',external:'External DMX'};
let syncMaster=false;
$('syncBtn').onclick=()=>{if(ws&&ws.readyState===1)ws.send(JSON.stringify({syncMaster:!syncMaster}));};
function updateSync(s){
  syncMaster=s.syncMaster;
  const btn=$('syncBtn');
  btn.textContent='Sync Master: '+(s.syncMaster?'On':'Off');
  btn.className=s.syncMaster?'':'sec';
  let info='';
  if(s.syncMaster)info=s.src==='manual'?'Sending Fan/Pump to other devices':'Paused - only active in manual mode';
  else if(s.syncRx)info='Synced from master';
  $('syncInfo').textContent=info;
}
function update(s){
  for(const k of keys){
    const c=ctl[k];
    if(!c.drag)c.r.value=s[k];
    if(document.activeElement!==c.n&&!c.drag)c.n.value=s[k];
    if(!c.drag)c.pct(s[k]);
  }
  const src=$('src');src.textContent=SRC[s.src]||s.src;src.className='badge'+(s.src!=='manual'?' net':'');
  const b=$('banner');
  if(s.src==='external'){
    b.textContent='External DMX active - Hazer is controlled by physical DMX input. Short-press the mode button to use ArtNet / manual control.';
    b.classList.remove('hide');
  }else if(s.src!=='manual'){
    b.textContent=`${SRC[s.src]} input active - manual control overridden. Output @${s.base}: Fan ${s.oFan}, Pump ${s.oPump}`;
    b.classList.remove('hide');
  }else b.classList.add('hide');
  renderPresets(s.presets);
  updateSync(s);
  $('foot').textContent=`DMX base address ${s.base} | v${s.ver}`;
}
function connect(){
  ws=new WebSocket(`ws://${location.host}/ws`);
  ws.onopen=()=>{$('conn').textContent='connected';};
  ws.onclose=()=>{$('conn').textContent='disconnected - retrying...';setTimeout(connect,2000);};
  ws.onmessage=e=>{try{update(JSON.parse(e.data));}catch(_){}}
}
connect();
</script>
</body></html>
)html";

static const char WIFI_HTML[] PROGMEM = PAGE_HEAD("WiFi") R"html(
<form id="f" onsubmit="return false">
<div class="card">
  <h2>WiFi Connection</h2>
  <label>Network name (SSID)</label>
  <div class="row"><input class="full" name="ssid" maxlength="32"><button type="button" class="sec" onclick="scan()">Scan</button></div>
  <div id="scan"></div>
  <label>Password</label>
  <input class="full" type="password" name="pass" maxlength="63" autocomplete="new-password">
  <label>Hostname</label>
  <input class="full" id="hostname" readonly>
  <p class="hint">Derived from the device name (System page). Reachable as http://<span id="mdns"></span>.local</p>
  <label class="inl"><input type="checkbox" name="staticIp" id="staticIp" onchange="toggleIp()"> Static IP</label>
  <div id="ipbox" class="grid2">
    <div><label>IP address</label><input class="full" name="ip"></div>
    <div><label>Gateway</label><input class="full" name="gateway"></div>
    <div><label>Subnet mask</label><input class="full" name="subnet"></div>
    <div><label>DNS</label><input class="full" name="dns"></div>
  </div>
</div>
<div class="card">
  <h2>Access Point</h2>
  <p class="hint">Opened automatically when no WiFi connection is available. Captive portal at 4.3.2.1.</p>
  <label>AP name (SSID)</label>
  <input class="full" name="apSsid" maxlength="32" required>
  <label>AP password (8-63 characters, empty = open)</label>
  <input class="full" type="password" name="apPass" maxlength="63" autocomplete="new-password">
  <div class="grid2">
    <div><label>Channel</label><input type="number" name="apChannel" min="1" max="13"></div>
    <div><label>&nbsp;</label><label class="inl"><input type="checkbox" name="apHidden"> Hide SSID</label></div>
  </div>
</div>
<button class="full" onclick="save()">Save &amp; Reboot</button>
<div id="msg" class="msg"></div>
</form>
</main>
<script>
const f=$('f');
function toggleIp(){$('ipbox').classList.toggle('hide',!$('staticIp').checked);}
async function load(){
  const c=await getJson('/api/config');fill(f,c);$('hostname').value=c.hostname;$('mdns').textContent=c.hostname;toggleIp();
}
async function save(){
  if(await submitForm(f,'/api/wifi',$('msg')))
    setTimeout(()=>{$('msg').textContent+=' Reconnect to the new network/address if it changed.';},1500);
}
let scanTimer;
async function scan(){
  const box=$('scan');box.textContent='Scanning...';
  clearTimeout(scanTimer);
  try{
    const r=await getJson('/api/scan');
    if(r.scanning){scanTimer=setTimeout(scan,1500);return;}
    box.innerHTML='';
    r.networks.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
      const d=document.createElement('div');
      d.textContent=`${n.ssid} (${n.rssi} dBm${n.open?', open':''})`;
      d.onclick=()=>{f.ssid.value=n.ssid;f.pass.value='';box.innerHTML='';f.pass.focus();};
      box.appendChild(d);
    });
    if(!r.networks.length)box.textContent='No networks found';
  }catch(e){box.textContent='Scan failed';}
}
load();
</script>
</body></html>
)html";

static const char DMX_HTML[] PROGMEM = PAGE_HEAD("DMX") R"html(
<form id="f" onsubmit="return false">
<div class="card">
  <h2>DMX Output</h2>
  <label>Base address (Fan = base, Pump = base + 1)</label>
  <input type="number" name="base" min="1" max="511" required>
  <p class="hint">In manual mode only these two channels are sent, all others are 0. Valid ArtNet/sACN input is passed through 1:1 (full universe) and has priority.</p>
</div>
<div class="card">
  <h2>ArtNet</h2>
  <label class="inl"><input type="checkbox" name="artnet"> Enable ArtNet input</label>
  <div class="grid2" style="grid-template-columns:1fr 1fr 1fr">
    <div><label>Net (0-127)</label><input type="number" name="artNet" min="0" max="127" oninput="pa()"></div>
    <div><label>Subnet (0-15)</label><input type="number" name="artSubnet" min="0" max="15" oninput="pa()"></div>
    <div><label>Universe (0-15)</label><input type="number" name="artUniverse" min="0" max="15" oninput="pa()"></div>
  </div>
  <p class="hint">Port-Address: <span id="pa">-</span></p>
  <label>Short name (ArtPoll reply)</label>
  <input class="full" name="shortName" maxlength="17">
  <label>Long name (ArtPoll reply)</label>
  <input class="full" name="longName" maxlength="63">
</div>
<div class="card">
  <h2>sACN (E1.31)</h2>
  <label class="inl"><input type="checkbox" name="sacn"> Enable sACN input</label>
  <label>Universe (1-63999)</label>
  <input type="number" name="sacnUniverse" min="1" max="63999">
</div>
<div class="card">
  <h2>Input Timeout</h2>
  <label>Fall back to manual control after (seconds)</label>
  <input type="number" name="timeout" min="1" max="60">
</div>
<div class="card">
  <h2>Quick Select Presets</h2>
  <div class="grid3"><span class="hint">Name</span><span class="hint">Fan</span><span class="hint">Pump</span></div>
  <div id="presets"></div>
</div>
<div class="card">
  <h2>Sync</h2>
  <p class="hint">Synchronises Fan and Pump between devices in the same network. The Sync Master is switched on the Control page. Only active in manual mode - ArtNet, sACN and External DMX have priority. Each device outputs the values at its own DMX base address.</p>
  <label class="inl"><input type="checkbox" name="syncRecv"> Receive sync from a master</label>
  <label>UDP port</label>
  <input type="number" name="syncPort" min="1024" max="65535">
  <label>Packets sent per change (1-10)</label>
  <input type="number" name="syncRepeat" min="1" max="10" required>
  <p class="hint">Higher values make sync more robust against lost WiFi packets. The 1 s keepalive is always sent once.</p>
  <label>Send groups (used when this device is Sync Master)</label>
  <div id="sg" class="groups"></div>
  <label>Receive groups</label>
  <div id="rg" class="groups"></div>
</div>
<button class="full" onclick="submitForm(f,'/api/dmx',$('msg'))">Save</button>
<div id="msg" class="msg"></div>
</form>
</main>
<script>
const f=$('f');
function pa(){
  const n=+f.artNet.value||0,s=+f.artSubnet.value||0,u=+f.artUniverse.value||0;
  $('pa').textContent=`${n*256+s*16+u} (0x${(n*256+s*16+u).toString(16).padStart(4,'0')})`;
}
async function load(){
  const c=await getJson('/api/config');
  for(const pre of ['sg','rg'])
    for(let g=1;g<=8;g++)
      $(pre).insertAdjacentHTML('beforeend',`<label class="inl"><input type="checkbox" name="${pre}${g}"> ${g}</label>`);
  const box=$('presets');
  for(let i=0;i<c.presetCount;i++){
    const row=document.createElement('div');row.className='grid3';row.style.marginBottom='6px';
    row.innerHTML=`<input name="p${i}n" maxlength="16" required><input type="number" name="p${i}f" min="0" max="255"><input type="number" name="p${i}p" min="0" max="255">`;
    box.appendChild(row);
  }
  fill(f,c);pa();
}
load();
</script>
</body></html>
)html";

static const char SYSTEM_HTML[] PROGMEM = PAGE_HEAD("System") R"html(
<div class="card">
  <h2>Info</h2>
  <div id="info" class="hint">-</div>
</div>
<div class="card">
  <h2>Device Name</h2>
  <form id="nf" onsubmit="return false">
    <div class="row">
      <input class="full" name="deviceName" id="deviceName" maxlength="32" placeholder="e.g. Stage Left Fogger">
      <button onclick="saveName()">Save</button>
    </div>
  </form>
  <p class="hint">Shown in the header of every page and used as network hostname (currently <b id="hn"></b>). Changing the hostname reboots the device.</p>
  <div id="nmsg" class="msg"></div>
</div>
<div class="card">
  <h2>Firmware Update</h2>
  <input type="file" id="fw" accept=".bin" class="full">
  <p class="hint">DMX output is paused during the update.</p>
  <button class="full" onclick="upload()">Upload &amp; Install</button>
  <progress id="prog" max="100" value="0" class="full hide" style="width:100%;margin-top:10px"></progress>
  <div id="msg" class="msg"></div>
</div>
<div class="card">
  <h2>Maintenance</h2>
  <div class="row">
    <button class="sec" onclick="postCmd('/api/reboot','Reboot device?')">Reboot</button>
    <button class="dang" onclick="postCmd('/api/factory','Reset ALL settings to factory defaults?')">Factory Reset</button>
  </div>
  <p class="hint">Factory reset is also possible by holding the BOOT button for 5 seconds.</p>
</div>
</main>
<script>
getJson('/api/name').then(n=>{$('deviceName').value=n.name;$('hn').textContent=n.hostname;});
async function saveName(){if(await submitForm($('nf'),'/api/name',$('nmsg')))setTimeout(loadName,300);}
getJson('/api/info').then(i=>{
  $('info').innerHTML='';
  for(const [k,v] of Object.entries(i)){const d=document.createElement('div');d.textContent=`${k}: ${v}`;$('info').appendChild(d);}
});
function upload(){
  const file=$('fw').files[0];if(!file){alert('Select a firmware .bin file');return;}
  const fd=new FormData();fd.append('update',file,file.name);
  const x=new XMLHttpRequest();const p=$('prog');p.classList.remove('hide');
  x.upload.onprogress=e=>{if(e.lengthComputable)p.value=e.loaded/e.total*100;};
  x.onload=()=>{$('msg').textContent=x.responseText;};
  x.onerror=()=>{$('msg').textContent='Upload failed';};
  x.open('POST','/update');x.send(fd);
}
</script>
</body></html>
)html";
