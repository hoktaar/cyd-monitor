// Web-Oberflaeche (eine Seite, spricht mit /api/state, /api/settings, /api/scan, /api/restart).
#pragma once

const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD Monitor</title>
<style>
:root{--bg:#f4f3ef;--card:#fff;--text:#16181e;--muted:#646a78;--line:#e2e3e8;--accent:#d97757}
@media (prefers-color-scheme:dark){:root{--bg:#0a0c12;--card:#171a23;--text:#ebeef5;--muted:#828a9b;--line:#262a36}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,sans-serif}
main{max-width:560px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:22px;margin:4px 0 2px}h1 span{color:var(--accent)}.sub{color:var(--muted);margin:0 0 18px}
section{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:14px}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--muted);margin:0 0 12px}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:8px 0;border-top:1px solid var(--line)}
.row:first-of-type{border-top:0}label{flex:1}
select,input[type=text],input[type=password],input[type=number]{font:inherit;color:var(--text);background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:7px 10px;min-width:0}
input[type=number]{width:80px}input[type=range]{flex:1;max-width:220px;accent-color:var(--accent)}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--accent)}
.seg{display:inline-flex;border:1px solid var(--line);border-radius:9px;overflow:hidden}
.seg button{border:0;background:transparent;color:var(--text);padding:7px 14px;font:inherit;cursor:pointer}
.seg button.on{background:var(--accent);color:#fff}
.btn{font:inherit;border:0;border-radius:9px;padding:9px 16px;background:var(--accent);color:#fff;cursor:pointer}
.btn.ghost{background:transparent;color:var(--text);border:1px solid var(--line)}
.stack{display:flex;flex-direction:column;gap:8px}.stack input{width:100%}
.stat{display:grid;grid-template-columns:auto 1fr;gap:4px 14px}.stat dt{color:var(--muted)}.stat dd{margin:0;overflow-wrap:anywhere}
.nets{display:flex;flex-direction:column;gap:4px}.nets:empty{display:none}
.nets button{display:flex;justify-content:space-between;font:inherit;color:var(--text);background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:9px 12px;cursor:pointer;text-align:left}
.nets button.on{border-color:var(--accent)}.nets small{color:var(--muted)}
progress{width:100%;accent-color:var(--accent)}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:var(--text);color:var(--bg);padding:8px 14px;border-radius:9px;opacity:0;transition:.2s}
#toast.show{opacity:1}
</style></head><body><main>
<h1><span>&#9679;</span> CYD Monitor</h1><p class="sub" id="sub">Lade ...</p>

<section><h2>Anzeige</h2>
<div class="row"><label>Design</label><div class="seg" id="theme"><button data-v="0">Dunkel</button><button data-v="1">Hell</button></div></div>
<div class="row"><label for="bright">Helligkeit</label><input type="range" id="bright" min="10" max="255"></div>
<div class="row"><label>USB-Anschluss</label><div class="seg" id="orient"><button data-v="1">Links</button><button data-v="0">Rechts</button></div></div>
<div class="row"><label for="invert">Farben invertieren</label><input type="checkbox" id="invert"></div>
</section>

<section><h2>Slides</h2>
<div class="row"><label for="cycle">Wechsel alle ... Sekunden (0 = aus)</label><input type="number" id="cycle" min="0" max="3600"></div>
<div id="pages"></div>
</section>

<div id="mods"></div>

<section><h2>Uhrzeit</h2>
<div class="row"><label for="tz">Zeitzone</label><select id="tz">
<option value="CET-1CEST,M3.5.0,M10.5.0/3">Deutschland / &Ouml;sterreich / Schweiz</option>
<option value="GMT0BST,M3.5.0/1,M10.5.0">Gro&szlig;britannien</option>
<option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Osteuropa (EET)</option>
<option value="UTC0">UTC</option></select></div>
</section>

<section><h2>WLAN</h2>
<div class="stack">
<div id="nets" class="nets"></div>
<input type="text" id="ssid" placeholder="Netzwerkname (SSID)" autocomplete="off">
<input type="password" id="pass" placeholder="Passwort" autocomplete="new-password">
<div><button class="btn" id="save-wifi">Speichern &amp; verbinden</button> <button class="btn ghost" id="scan">Netzwerke suchen</button></div>
</div></section>

<section><h2>Firmware-Update</h2>
<div class="stack">
<div class="row"><label>Installiert</label><b id="fwver">-</b></div>
<input type="text" id="repo" placeholder="GitHub-Repository (benutzer/cyd-monitor)" autocomplete="off">
<p id="otamsg" class="sub" style="margin:0"></p>
<div><button class="btn" id="otacheck">Nach Updates suchen</button> <button class="btn" id="otainstall" hidden>Installieren</button></div>
<details><summary class="sub" style="margin:8px 0;cursor:pointer">Manuell: .bin-Datei hochladen</summary>
<div class="stack"><input type="file" id="fw" accept=".bin">
<progress id="fwp" max="100" value="0" hidden></progress>
<div><button class="btn ghost" id="upload">Hochladen &amp; installieren</button></div></div></details>
</div></section>

<section><h2>Status</h2><dl class="stat" id="status"></dl>
<p><button class="btn ghost" id="restart">Neu starten</button></p></section>
</main><div id="toast"></div>
<script>
const $=id=>document.getElementById(id);let S={};
function toast(t){$('toast').textContent=t;$('toast').classList.add('show');clearTimeout(toast.t);toast.t=setTimeout(()=>$('toast').classList.remove('show'),1600)}
async function post(data){const r=await fetch('/api/settings',{method:'POST',body:new URLSearchParams(data)});S=await r.json();render();toast('Gespeichert')}
function seg(id,key){$(id).querySelectorAll('button').forEach(b=>b.onclick=()=>post({[key]:b.dataset.v}))}
function mark(id,v){$(id).querySelectorAll('button').forEach(b=>b.classList.toggle('on',b.dataset.v==String(v)))}
function render(){
 mark('theme',S.theme);mark('orient',S.orient);
 if(document.activeElement!==$('bright'))$('bright').value=S.bright;
 $('invert').checked=!!S.invert;
 if(document.activeElement!==$('cycle'))$('cycle').value=S.cycle;
 $('tz').value=S.tz;
 $('pages').innerHTML=S.modules.map((m,i)=>`<div class="row"><label for="p${i}">${m}${S.ready[i]?'':' <small class="sub">(nicht eingerichtet)</small>'}</label><input type="checkbox" id="p${i}" ${S.pages>>i&1?'checked':''}></div>`).join('');
 renderMods();
 S.modules.forEach((m,i)=>$('p'+i).onchange=()=>{let v=0;S.modules.forEach((_,j)=>{if($('p'+j).checked)v|=1<<j});post({'cfg.pages':v})});
 if(!$('ssid').value)$('ssid').value=S.ssid||'';
 $('fwver').textContent=S.version;
 if(document.activeElement!==$('repo'))$('repo').value=S.repo||'';
 const w=S.wifi=='ok'?`verbunden mit ${S.ssid}`:S.wifi=='ap'?'Einrichtungsmodus (Hotspot)':'verbinde ...';
 $('sub').textContent=`WLAN ${w}`;
 const pc=S.pcAgo<0?'noch keine Daten':S.pcAgo<90?'verbunden':`vor ${Math.round(S.pcAgo/60)} min`;
 const rows=[['IP-Adresse',S.ip||'-'],['Adresse',`http://${S.host}.local`],['Signal',S.wifi=='ok'?S.rssi+' dBm':'-'],['PC-Daten',pc],['Laufzeit',Math.floor(S.uptime/3600)+' h '+Math.floor(S.uptime%3600/60)+' min'],['Firmware',S.version]];
 $('status').innerHTML=rows.map(r=>`<dt>${r[0]}</dt><dd>${r[1]}</dd>`).join('');
}
const esc=s=>String(s??'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function renderMods(){
 const box=$('mods');
 // Felder nur einmal aufbauen (sonst gehen Eingaben verloren), danach nur Status aktualisieren
 if(box.dataset.built){S.config.forEach((m,i)=>{const s=$('ms'+i);if(s)s.textContent=m.status||''});return}
 box.dataset.built=1;
 box.innerHTML=S.config.map((m,i)=>`<section><h2>${esc(m.title)}</h2><div class="stack">`+m.fields.map(f=>f.type=='location'?
  `<div class="row"><label>${esc(f.label)}</label><b id="loc-${f.key}">${esc(f.value)||'-'}</b></div>
   <input type="text" id="q-${f.key}" placeholder="${esc(f.hint)}"><div class="nets" id="r-${f.key}"></div>
   <div><button class="btn ghost" type="button" data-loc="${f.key}">Ort suchen</button></div>`:
  `<label class="sub" style="margin:4px 0 -4px">${esc(f.label)}</label>
   <input type="${f.type=='password'?'password':f.type=='number'?'number':'text'}" data-key="${f.key}" data-mod="${i}" value="${f.type=='password'?'':esc(f.value)}"
    placeholder="${f.type=='password'&&f.set?'gespeichert - leer lassen = unverändert':esc(f.hint)}" autocomplete="off">`).join('')+
  (m.fields.some(f=>f.type!='location')?`<div><button class="btn" type="button" data-save="${i}">Speichern</button></div>`:'')+
  `<p class="sub" id="ms${i}" style="margin:0">${esc(m.status)}</p></div></section>`).join('');
 box.querySelectorAll('[data-save]').forEach(b=>b.onclick=()=>{const d={};
  box.querySelectorAll(`[data-mod="${b.dataset.save}"]`).forEach(inp=>{if(inp.type!='password'||inp.value)d['mod.'+inp.dataset.key]=inp.value});
  post(d)});
 box.querySelectorAll('[data-loc]').forEach(b=>b.onclick=async()=>{const k=b.dataset.loc,q=$('q-'+k).value.trim();if(!q)return toast('Bitte Ort eingeben');
  try{const r=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=6&language=de&name='+encodeURIComponent(q))).json();
   const list=$('r-'+k);list.innerHTML='';(r.results||[]).forEach(p=>{const e=document.createElement('button');e.type='button';
    e.innerHTML=`<span></span><small></small>`;e.firstChild.textContent=p.name;e.lastChild.textContent=[p.admin1,p.country].filter(Boolean).join(', ');
    e.onclick=async()=>{await post({['mod.'+k+'.n']:p.name,['mod.'+k+'.la']:p.latitude.toFixed(4),['mod.'+k+'.lo']:p.longitude.toFixed(4)});
     $('loc-'+k).textContent=p.name;list.innerHTML=''};list.appendChild(e)});
   if(!(r.results||[]).length)toast('Kein Ort gefunden')}catch(e){toast('Ortssuche braucht Internet')}});
}
seg('theme','cfg.theme');seg('orient','cfg.orient');
$('bright').onchange=e=>post({'cfg.bright':e.target.value});
$('invert').onchange=e=>post({'cfg.invert':e.target.checked?1:0});
$('cycle').onchange=e=>post({'cfg.cycle':e.target.value});
$('tz').onchange=e=>post({'cfg.tz':e.target.value});
$('scan').onclick=async()=>{toast('Suche ...');const all=await (await fetch('/api/scan')).json();
 const seen=new Set(),n=all.filter(x=>x.ssid&&!seen.has(x.ssid)&&seen.add(x.ssid)).sort((a,b)=>b.rssi-a.rssi);
 const box=$('nets');box.innerHTML='';
 n.forEach(x=>{const b=document.createElement('button');b.type='button';b.innerHTML=`<span></span><small>${x.open?'offen · ':''}${x.rssi} dBm</small>`;b.firstChild.textContent=x.ssid;
  b.onclick=()=>{$('ssid').value=x.ssid;box.querySelectorAll('button').forEach(e=>e.classList.toggle('on',e===b));$('pass').focus()};box.appendChild(b)});
 toast(n.length+' Netzwerke gefunden')};
$('repo').onchange=e=>post({'cfg.repo':e.target.value});
function ota(o){$('otainstall').hidden=!o.available;
 $('otamsg').textContent=o.error?o.error:o.available?`Version ${o.latest} ist verfügbar.`:o.latest?`Aktuell - neueste Version ist ${o.latest}.`:''}
$('otacheck').onclick=async()=>{$('otamsg').textContent='Frage GitHub ...';ota(await (await fetch('/api/ota/check')).json())};
$('otainstall').onclick=async()=>{await fetch('/api/ota/install',{method:'POST'});$('otainstall').hidden=true;
 $('otamsg').textContent='Update läuft - Fortschritt auf dem Display. Die Seite lädt danach neu.';setTimeout(()=>location.reload(),45000)};
$('upload').onclick=()=>{const f=$('fw').files[0];if(!f)return toast('Bitte .bin-Datei wählen');
 const x=new XMLHttpRequest(),fd=new FormData();fd.append('firmware',f);$('fwp').hidden=false;
 x.upload.onprogress=e=>{if(e.lengthComputable)$('fwp').value=e.loaded/e.total*100};
 x.onload=()=>{toast(x.status==200?'Installiert - startet neu ...':'Fehler: '+x.responseText);setTimeout(()=>location.reload(),8000)};
 x.onerror=()=>toast('Upload fehlgeschlagen');x.open('POST','/api/update');x.send(fd)};
$('save-wifi').onclick=async()=>{if(!$('ssid').value)return toast('Bitte SSID eingeben');await post({'wifi.ssid':$('ssid').value,'wifi.pass':$('pass').value});toast('Verbinde neu ...')};
$('restart').onclick=async()=>{await fetch('/api/restart',{method:'POST'});toast('Startet neu ...')};
async function load(){try{S=await (await fetch('/api/state')).json();render()}catch(e){$('sub').textContent='Keine Verbindung zum Board'}}
load();setInterval(load,5000);
</script></body></html>)HTML";
