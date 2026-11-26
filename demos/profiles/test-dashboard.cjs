'use strict';
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { spawn } = require('node:child_process');

const edge = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
if (!fs.existsSync(edge)) { console.error('Microsoft Edge is not installed at the checked path.'); process.exit(2); }
const tempBase = path.resolve(os.tmpdir());
const prefix = path.join(tempBase, 'spice-profiles-qa-');
const tempDir = fs.mkdtempSync(prefix);
if (!path.resolve(tempDir).startsWith(prefix)) throw Error('Unexpected temporary directory');
const html = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8');
const avatarFixtures=JSON.parse(fs.readFileSync(path.join(__dirname,'../../tests/fixtures/profile-avatars.json'),'utf8'));
const test = String.raw`<script>
(async () => {
  const results=[];
  const avatars=${JSON.stringify(avatarFixtures)};
  const check=(condition,label)=>{if(!condition)throw Error(label);results.push(label);};
  const card=platform=>document.querySelector('[aria-label="'+platform+' profile"]');
  const click=(scope,label)=>{const b=[...scope.querySelectorAll('button')].find(x=>x.textContent===label);if(!b)throw Error('Missing button: '+label);b.click();};
  try {
    check(card('PlayStation').textContent.includes('Setup needed'),'PSN starts setup-needed');
    check(card('PlayStation').textContent.includes('No account identity supplied'),'no PSN identity guessed');
    check(card('Xbox').textContent.includes('pikldspratmn'),'owner-supplied Xbox handle visible');
    check(card('Xbox').textContent.includes('No verified public data'),'Xbox unavailable status visible');
    check(card('Xbox').textContent.includes('Owner supplied · unverified'),'handle provenance visible');
    check(card('Xbox').querySelector('a').href==='https://www.xbox.com/en-US/play/user?gamertag=pikldspratmn','profile link uses fixed official origin');
    document.getElementById('sample-button').click();
    check(card('PlayStation').textContent.includes('Sample only') && card('Xbox').textContent.includes('18,250'),'sample profiles are visible and labelled');
    check(!card('PlayStation').querySelector('a'),'sample does not invent a PSN identity');
    document.getElementById('reset-button').click();
    check(card('Xbox').textContent.includes('pikldspratmn'),'starting view restores owner handle');
    const attack='<img src=x onerror=alert(1)>';
    const item=(value)=>({value,source:'imported',observed_at:null});
    const payload={schema:'spice.profiles/1',generated_at:null,profiles:{xbox:{state:'imported',fields:{handle:item('PlayerOne'),display_name:item(attack),gamerscore:item(42),recent_games:item(['Example game'])},last_attempt:null},psn:{state:'setup_needed',fields:{},last_attempt:null}}};
    const input=document.getElementById('file-input');Object.defineProperty(input,'files',{configurable:true,value:[new File([JSON.stringify(payload)],'example.json',{type:'application/json'})]});
    input.dispatchEvent(new Event('change',{bubbles:true}));
    for(let i=0;i<100 && document.getElementById('mode-title').textContent!=='Local snapshot imported';i++) await new Promise(resolve=>setTimeout(resolve,20));
    check(card('Xbox').querySelector('h2').textContent===attack && !card('Xbox').querySelector('img'),'untrusted HTML renders as text (status: '+document.getElementById('notice').textContent+')');
    check(card('Xbox').textContent.includes('42') && card('Xbox').textContent.includes('Imported'),'imported stats and source visible');
    let captured=null;URL.createObjectURL=blob=>{captured=blob;return 'blob:local-test';};URL.revokeObjectURL=()=>{};HTMLAnchorElement.prototype.click=function(){};
    document.getElementById('export-button').click();
    check(!!captured,'export creates a local JSON blob');
    const exported=JSON.parse(await captured.text());check(exported.profiles.xbox.fields.gamerscore.value===42,'export preserves imported value');
    window.alert=()=>{};
    Object.defineProperty(input,'files',{configurable:true,value:[new File(['{bad'],'bad.json',{type:'application/json'})]});
    input.dispatchEvent(new Event('change',{bubbles:true}));
    for(let i=0;i<100 && !document.getElementById('notice').textContent.startsWith('Import failed:');i++) await new Promise(resolve=>setTimeout(resolve,20));
    check(document.getElementById('notice').textContent.startsWith('Import failed:') && card('Xbox').textContent.includes('42'),'malformed import preserves displayed data');
    const remote=JSON.parse(JSON.stringify(payload));remote.profiles.xbox.fields.avatar=item('https://evil.test/picture.png');
    Object.defineProperty(input,'files',{configurable:true,value:[new File([JSON.stringify(remote)],'remote.json',{type:'application/json'})]});
    document.getElementById('notice').textContent='';input.dispatchEvent(new Event('change',{bubbles:true}));
    for(let i=0;i<100 && !document.getElementById('notice').textContent.startsWith('Import failed:');i++) await new Promise(resolve=>setTimeout(resolve,20));
    check(document.getElementById('notice').textContent.startsWith('Import failed:') && !card('Xbox').querySelector('img'),'remote avatar import is rejected');
    for(const kind of ['png','jpeg','webp']){
      const withAvatar=JSON.parse(JSON.stringify(payload));withAvatar.profiles.xbox.fields.avatar=item(avatars[kind]);
      document.getElementById('mode-title').textContent='';
      Object.defineProperty(input,'files',{configurable:true,value:[new File([JSON.stringify(withAvatar)],kind+'.json',{type:'application/json'})]});
      input.dispatchEvent(new Event('change',{bubbles:true}));
      for(let i=0;i<100&&document.getElementById('mode-title').textContent!=='Local snapshot imported';i++)await new Promise(resolve=>setTimeout(resolve,20));
      const image=card('Xbox').querySelector('img');if(image)await image.decode();
      check(image&&image.naturalWidth===128&&image.naturalHeight===128,'static '+kind+' avatar renders at bounded source size');
    }
    for(const kind of ['oversized_png','animated_webp']){
      const badAvatar=JSON.parse(JSON.stringify(payload));badAvatar.profiles.xbox.fields.avatar=item(avatars[kind]);
      document.getElementById('notice').textContent='';
      Object.defineProperty(input,'files',{configurable:true,value:[new File([JSON.stringify(badAvatar)],kind+'.json',{type:'application/json'})]});
      input.dispatchEvent(new Event('change',{bubbles:true}));
      for(let i=0;i<100&&!document.getElementById('notice').textContent.startsWith('Import failed:');i++)await new Promise(resolve=>setTimeout(resolve,20));
      check(document.getElementById('notice').textContent.startsWith('Import failed:')&&card('Xbox').querySelector('img').src===avatars.webp,kind+' is rejected before replacing the last good avatar');
    }
    // A bounded JPEG SOF with no scan has admissible dimensions but cannot decode.
    // A damaged PNG was unsuitable here because Chromium recovered its pixels.
    const broken=String.fromCharCode(255,216,255,192,0,17,8,0,128,0,128,3,1,17,0,2,17,0,3,17,0,255,217);
    const badCodec=JSON.parse(JSON.stringify(payload));badCodec.profiles.xbox.fields.avatar=item('data:image/jpeg;base64,'+btoa(broken));
    Object.defineProperty(input,'files',{configurable:true,value:[new File([JSON.stringify(badCodec)],'codec.json',{type:'application/json'})]});
    input.dispatchEvent(new Event('change',{bubbles:true}));
    for(let i=0;i<100&&card('Xbox').querySelector('.avatar').textContent!=='X';i++)await new Promise(resolve=>setTimeout(resolve,20));
    check(card('Xbox').querySelector('.avatar').textContent==='X'&&!card('Xbox').querySelector('img'),'bounded undecodable avatar falls back to platform initials: '+JSON.stringify({text:card('Xbox').querySelector('.avatar').textContent,notice:document.getElementById('notice').textContent,width:card('Xbox').querySelector('img')?.naturalWidth}));
    const badDate=JSON.parse(JSON.stringify(payload));badDate.generated_at='2026-02-30T12:00:00Z';
    Object.defineProperty(input,'files',{configurable:true,value:[new File([JSON.stringify(badDate)],'date.json',{type:'application/json'})]});
    document.getElementById('notice').textContent='';input.dispatchEvent(new Event('change',{bubbles:true}));
    for(let i=0;i<100 && !document.getElementById('notice').textContent.startsWith('Import failed:');i++) await new Promise(resolve=>setTimeout(resolve,20));
    check(document.getElementById('notice').textContent.startsWith('Import failed:') && card('Xbox').textContent.includes('42'),'invalid timestamp import is rejected');
    click(card('Xbox'),'Change handle');const editor=card('Xbox').querySelector('form');editor.querySelector('input').value='AnotherPlayer';editor.dispatchEvent(new Event('submit',{bubbles:true,cancelable:true}));
    check(card('Xbox').textContent.includes('AnotherPlayer') && !card('Xbox').textContent.includes('Example game'),'changing identity removes old details');
    check(document.activeElement.textContent==='Change handle','focus returns to the updated profile control');
    const first=document.getElementById('import-button');first.focus();first.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowRight',bubbles:true}));
    check(document.activeElement!==first,'arrow-key focus moves');
    check(performance.getEntriesByType('resource').every(entry=>!/^https?:/i.test(entry.name)),'page loaded no network resources');
    document.documentElement.dataset.qa='pass';
  } catch(error) {results.push('FAIL: '+error.message);document.documentElement.dataset.qa='fail';}
  const output=document.createElement('pre');output.id='qa-results';output.textContent=results.join('\n');document.body.append(output);
})();
</script>`;
fs.writeFileSync(path.join(tempDir, 'test.html'), html.replace('</body>', test + '</body>'));
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));

async function run() {
  const profileDir=path.join(tempDir,'edge-profile');
  // Edge's Windows GUI launcher detaches immediately. Use its isolated local
  // DevTools connection for QA; the file itself is never served over HTTP.
  const args=['--headless=new','--disable-gpu','--disable-extensions','--no-first-run','--no-default-browser-check','--disable-background-networking','--disable-component-update','--disable-sync','--remote-debugging-port=0',`--user-data-dir=${profileDir}`,'about:blank'];
  const child=spawn(edge,args,{windowsHide:true,stdio:'ignore'});
  let launchError=null;child.on('error',error=>{launchError=error;});
  let socket, send;
  try {
    const active=path.join(profileDir,'DevToolsActivePort');
    for(let i=0;i<150 && !fs.existsSync(active);i++){if(launchError)throw launchError;await pause(100);}
    if(!fs.existsSync(active))throw Error('Isolated Edge did not open its local debugging connection.');
    const [port,wsPath]=fs.readFileSync(active,'utf8').trim().split(/\r?\n/);
    if(!/^\d+$/.test(port) || !/^\/devtools\/browser\/[a-zA-Z0-9-]+$/.test(wsPath))throw Error('Invalid local debugging endpoint.');
    socket=new WebSocket(`ws://127.0.0.1:${port}${wsPath}`);
    await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});setTimeout(()=>reject(Error('Edge connection timed out.')),5000).unref();});
    let nextId=1;const pending=new Map();
    socket.addEventListener('message',event=>{const msg=JSON.parse(event.data);if(!msg.id || !pending.has(msg.id))return;const entry=pending.get(msg.id);pending.delete(msg.id);clearTimeout(entry.timer);if(msg.error)entry.reject(Error(msg.error.message));else entry.resolve(msg.result);});
    send=(method,params={},sessionId)=>new Promise((resolve,reject)=>{const id=nextId++;const timer=setTimeout(()=>{pending.delete(id);reject(Error(method+' timed out.'));},8000);pending.set(id,{resolve,reject,timer});socket.send(JSON.stringify({id,method,params,...(sessionId?{sessionId}:{})}));});
    const target=await send('Target.createTarget',{url:pathToFileURL(path.join(tempDir,'test.html')).href});
    const attached=await send('Target.attachToTarget',{targetId:target.targetId,flatten:true});
    const session=attached.sessionId;await send('Runtime.enable',{},session);
    let outcome;
    for(let i=0;i<100;i++){
      const reply=await send('Runtime.evaluate',{expression:"JSON.stringify({status:document.documentElement.dataset.qa || '',results:document.getElementById('qa-results')?.textContent || ''})",returnByValue:true},session);
      outcome=JSON.parse(reply.result.value);if(outcome.status)break;await pause(100);
    }
    if(!outcome || !outcome.status)throw Error('Dashboard interaction checks did not complete.');
    console.log(outcome.results);if(outcome.status!=='pass')throw Error('Dashboard interaction check failed.');
    console.log('Dashboard browser check: PASS');
    if(process.argv.includes('--capture')){
      await send('Page.enable',{},session);
      await send('Page.navigate',{url:pathToFileURL(path.join(__dirname,'index.html')).href},session);
      for(let i=0;i<100;i++){const ready=await send('Runtime.evaluate',{expression:"document.readyState === 'complete' && !!document.querySelector('[aria-label=\\\"Xbox profile\\\"]')",returnByValue:true},session);if(ready.result.value)break;await pause(100);}
      for(const [name,width,height,scale,mobile] of [['qa-desktop.png',1440,1000,1,false],['qa-mobile.png',390,844,2,true],['qa-tv-4k.png',1920,1080,2,false]]){
        await send('Emulation.setDeviceMetricsOverride',{width,height,deviceScaleFactor:scale,mobile},session);
        const dimensions=await send('Runtime.evaluate',{expression:"JSON.stringify({width:innerWidth,scrollWidth:document.documentElement.scrollWidth})",returnByValue:true},session);
        const metrics=JSON.parse(dimensions.result.value);if(metrics.scrollWidth>metrics.width)throw Error(name+' has horizontal overflow.');
        const shot=await send('Page.captureScreenshot',{format:'png',captureBeyondViewport:false},session);
        fs.writeFileSync(path.join(__dirname,name),Buffer.from(shot.data,'base64'));console.log('Captured '+name);
      }
    }
  } catch(error) {console.error(error.message || error);process.exitCode=1;}
  finally {
    if(send){try{await send('Browser.close');}catch{ /* Closing the browser may close the socket before its reply. */ }}
    if(socket)socket.close();await pause(500);
    if(path.resolve(tempDir).startsWith(prefix)) fs.rmSync(tempDir,{recursive:true,force:true,maxRetries:5,retryDelay:200});
  }
}
run().catch(error=>{console.error(error);process.exitCode=1;});
