(() => {
  'use strict';
  const root = document.body;
  const mode = root.dataset.mode;
  const $ = (id) => document.getElementById(id);
  const phaseNames = ['Corm Dormancy / Preparation','Floral Induction','Flowering','Vegetative Growth','Daughter-Corm Development','Dormancy / Cycle Transition'];
  const history = {air: [], rh: []};
  let lastState = null;
  let config = null;
  let loading = false;
  const showError = (message) => { const e=$('global-error'); e.textContent=message; e.hidden=!message; };
  async function requestJson(url, options={}) {
    const r = await fetch(url, {headers:{'Content-Type':'application/json'}, ...options});
    let data; try { data=await r.json(); } catch { throw new Error(`Invalid JSON response (${r.status})`); }
    if (!r.ok || data.ok === false) throw new Error(data.error || `Request failed (${r.status})`);
    return data;
  }
  const valueText=(v,digits=1)=>v===null||v===undefined||!Number.isFinite(Number(v))?'NO DATA':Number(v).toFixed(digits);
  function setStateDot(id,on){$(id).className='state-dot '+(on?'on':'off');}
  function renderState(s) {
    lastState=s;
    $('connection-dot').className='dot '+(s.source==='ESP32'?'good':'');
    $('connection-text').textContent=s.source==='SIMULATED'?'Simulator running':'ESP32 reachable';
    $('source-tag').textContent=s.source==='SIMULATED'?'SIMULATION ONLY':'ESP32 DEVICE';
    $('phase-value').textContent=s.phase || phaseNames[s.phaseId] || 'Unknown';
    $('commission-button').hidden = !(s.configRecoveryRequired && s.source==='ESP32');
    $('mode-value').textContent=`Mode: ${s.mode || 'UNKNOWN'} · FW: ${s.firmwareVersion || 'unknown'}`;
    const sensor=s.sensors||{}; const out=s.outputs||{}; const ints=s.interlocks||{};
    $('air-value').textContent=valueText(sensor.airTemperatureC);
    $('rh-value').textContent=valueText(sensor.relativeHumidityPct);
    $('coolant-value').textContent=valueText(sensor.coolantTemperatureC);
    $('air-valid').textContent=sensor.dhtValid?'DHT22 valid':'DHT22 invalid / unavailable';
    $('rh-valid').textContent=sensor.dhtValid?'Freshness reported by controller':'Not a valid reading';
    $('coolant-valid').textContent=sensor.ds18b20Valid?'DS18B20 valid — confirm physical probe location':'DS18B20 invalid / unavailable';
    $('recovery-tag').textContent=s.recoveryStage||'UNKNOWN';
    $('recovery-value').textContent=s.recoveryStage||'—';
    $('save-value').textContent=s.configurationSaveStatus||'—';
    $('fresh-value').textContent=s.sensorDataFresh?'FRESH':'STALE / INVALID';
    $('lockout-reason').textContent=s.lockoutReason||'No reported reason';
    const pOn=!!out.peltier1CommandedOn; setStateDot('peltier-dot',pOn); $('peltier-state').textContent=pOn?'ON COMMAND':'OFF';
    setStateDot('pump-dot',!!out.pumpCommandedOn); $('pump-state').textContent=out.pumpCommandedOn?'ON COMMAND':'OFF';
    setStateDot('fan-dot',!!out.fanCommandedOn); $('fan-state').textContent=out.fanCommandedOn?'ON COMMAND':'OFF';
    setStateDot('mist-dot',!!out.mistCommandedOn); $('mist-state').textContent=out.mistCommandedOn?'ON COMMAND':'OFF';
    const fullySafe=s.recoveryComplete===true && !ints.estopLatched && s.source==='ESP32';
    $('safety-tag').textContent=fullySafe?'RECOVERY CONFIRMED':'INTERLOCK / NOT VERIFIED';
    $('safety-tag').className='tag '+(fullySafe?'good':'danger');
    $('previous-phase').textContent=s.lastSavedPhase||'Not separately reported';
    $('previous-mode').textContent=s.previousMode||'UNKNOWN';
    $('boot-stage').textContent=s.recoveryStage||'—';
    $('sensor-state').textContent=sensor.dhtValid&&sensor.ds18b20Valid?'Fresh readings reported':'Waiting / invalid';
    $('flow-state').textContent=ints.flowFeedbackInstalled?(ints.flowOkay?'Flow switch active':'Flow not confirmed'):'NOT INSTALLED / NOT VERIFIED';
    $('reservoir-state').textContent=ints.reservoirFeedbackInstalled?(ints.reservoirOkay?'Level switch OK':'Reservoir interlock failed'):'NOT INSTALLED / NOT VERIFIED';
    $('auto-resumed').textContent=s.recoveryComplete===true && s.automaticMode?'YES — firmware says complete':s.automaticMode?'NOT CONFIRMED':'NO / MANUAL';
    $('recovery-complete-tag').textContent=s.recoveryComplete===true?'FIRMWARE CONFIRMED':'NOT COMPLETE';
    $('recovery-complete-tag').className='tag '+(s.recoveryComplete===true?'good':'danger');
    const now=Date.now();
    history.air.push({t:now,v:sensor.dhtValid && Number.isFinite(Number(sensor.airTemperatureC))?Number(sensor.airTemperatureC):null});
    history.rh.push({t:now,v:sensor.dhtValid && Number.isFinite(Number(sensor.relativeHumidityPct))?Number(sensor.relativeHumidityPct):null});
    const limit=3600; history.air.splice(0,Math.max(0,history.air.length-limit)); history.rh.splice(0,Math.max(0,history.rh.length-limit));
    drawChart('temp-chart',history.air,'°C'); drawChart('rh-chart',history.rh,'%');
    const ev=(s.events||[]).slice(-8).reverse(); const tbody=$('events-body'); tbody.replaceChildren();
    if(!ev.length){const tr=document.createElement('tr');const td=document.createElement('td');td.colSpan=3;td.textContent='No event records returned for this session.';tr.append(td);tbody.append(tr);}
    for(const item of ev){const tr=document.createElement('tr');for(const v of [`${item.bootMs??'—'} ms`,item.kind||'EVENT',item.detail||'']){const td=document.createElement('td');td.textContent=String(v);tr.append(td);}tbody.append(tr);}
    if(Number.isInteger(Number(s.phaseId))) $('phase-select').value=String(s.phaseId);
  }
  function drawChart(id, points, unit) {
    const windowMs=Number($('chart-window')?.value||0); const cutoff=windowMs>0?Date.now()-windowMs:0;
    points=points.filter(p=>p.t>=cutoff);
    const validPoints=points.filter(p=>Number.isFinite(p.v));
    const canvas=$(id); const rect=canvas.getBoundingClientRect(); const dpr=window.devicePixelRatio||1;
    const w=Math.max(240,Math.floor(rect.width)); const h=180; canvas.width=w*dpr;canvas.height=h*dpr;
    const c=canvas.getContext('2d');c.scale(dpr,dpr);c.clearRect(0,0,w,h);c.strokeStyle='#34443b';c.fillStyle='#aab8ac';c.font='10px system-ui';
    const left=30,right=10,top=12,bottom=24,plotW=w-left-right,plotH=h-top-bottom;
    for(let i=0;i<4;i++){const y=top+i*plotH/3;c.beginPath();c.moveTo(left,y);c.lineTo(w-right,y);c.stroke();c.fillText('',3,y);}
    if(validPoints.length<2){c.fillText('Collecting valid readings…',left+8,top+plotH/2);return;}
    const vals=validPoints.map(p=>p.v),min=Math.min(...vals),max=Math.max(...vals),range=Math.max(.5,max-min);
    c.fillText(`${(max).toFixed(1)} ${unit}`,2,top+5);c.fillText(`${(min).toFixed(1)} ${unit}`,2,h-bottom);
    c.strokeStyle='#d5b36b';c.lineWidth=2;c.beginPath();
    let active=false; points.forEach((p,i)=>{if(!Number.isFinite(p.v)){active=false;return;}const x=left+i*plotW/Math.max(1,points.length-1),y=top+(max-p.v)/range*plotH;if(!active){c.moveTo(x,y);active=true;}else c.lineTo(x,y);});c.stroke();
  }
  function normalizeProfile(p) {
    return {airTargetC:p.airTargetC??22,airMinC:p.airMinC??18,airMaxC:p.airMaxC??26,
      rhTarget:p.rhTarget??p.rhTargetPct??60,rhMin:p.rhMin??p.rhMinPct??35,rhMax:p.rhMax??p.rhMaxPct??80,
      tempHyst:p.tempHyst??p.tempHysteresisC??1,rhHyst:p.rhHyst??p.rhHysteresisPct??5,
      mistPulseMs:p.mistPulseMs??2000,mistRestMs:p.mistRestMs??60000};
  }
  function setFormProfile(p) { const x=normalizeProfile(p); for(const k of Object.keys(x)) if($(k)) $(k).value=x[k]; $('profile-status').textContent='Loaded · unsaved'; }
  async function loadConfig() {
    try { config=await requestJson('/api/config'); $('profile-status').textContent=config.source==='SIMULATED'?'SIMULATED PROFILE':'Device configuration loaded';
      const select=$('phase-select');select.replaceChildren();phaseNames.forEach((p,i)=>{const opt=document.createElement('option');opt.value=i;opt.textContent=p;select.append(opt);});
      const profiles=config.profiles||[]; const phaseId=lastState?.phaseId??config.phase??1; if(profiles[phaseId])setFormProfile(profiles[phaseId]);
      select.addEventListener('change',()=>{const id=Number(select.value);if(profiles[id])setFormProfile(profiles[id]);});
    } catch(e) { $('profile-status').textContent='Unavailable'; showError(`Configuration could not be loaded: ${e.message}`); }
  }
  async function sendAction(payload, confirmText) {
    if(confirmText && !confirm(confirmText)) return;
    try { const result=await requestJson('/api/action',{method:'POST',body:JSON.stringify(payload)}); showError('');
      if(result.state)renderState({...result.state,source:'ESP32'}); else await refresh();
      return result;
    } catch(e){showError(e.message);throw e;}
  }
  async function refresh() {
    if(loading)return;loading=true;
    try { const state=await requestJson('/api/state'); showError(''); renderState(state); }
    catch(e){$('connection-dot').className='dot bad';$('connection-text').textContent='Device unavailable';showError(e.message);}
    finally{loading=false;}
  }
  $('commission-button').addEventListener('click',async()=>{try{await sendAction({action:'commission',confirm:true,commissioningAcknowledged:true},'Create the first saved configuration using provisional default profiles? This does not enable physical outputs.');await loadConfig();await refresh();}catch{}});
  $('commit-phase').addEventListener('click',async()=>{try{await sendAction({action:'phase',phaseId:Number($('phase-select').value)},`Commit phase “${phaseNames[Number($('phase-select').value)]}” to controller? The change will be stored by the device.`);}catch{}});
  $('auto-button').addEventListener('click',async()=>{try{await sendAction({action:'mode',automaticMode:true},'Request automatic mode? Physical outputs still require valid sensors and commissioned hardware.');}catch{}});
  $('manual-button').addEventListener('click',async()=>{try{await sendAction({action:'mode',automaticMode:false},'Switch to manual mode and force outputs off?');}catch{}});
  $('estop-button').addEventListener('click',async()=>{try{await sendAction({action:'estop'},'Latch the emergency stop? This requires deliberate fault clearance and will force controller outputs low.');}catch{}});
  $('save-profile').addEventListener('click',async()=>{
    const phaseId=Number($('phase-select').value);const profile={};
    for(const k of ['airTargetC','airMinC','airMaxC','rhTarget','rhMin','rhMax','tempHyst','rhHyst','mistPulseMs','mistRestMs']) profile[k]=Number($(k).value);
    if(Object.values(profile).some(v=>!Number.isFinite(v))){showError('Every profile field must contain a valid number.');return;}
    if(!(profile.airMinC<profile.airMaxC&&profile.airMinC<=profile.airTargetC&&profile.airTargetC<=profile.airMaxC&&profile.rhMin<profile.rhMax&&profile.rhMin<=profile.rhTarget&&profile.rhTarget<=profile.rhMax)){showError('Target must sit inside min/max bounds; min must be below max.');return;}
    try {const result=await requestJson('/api/profile',{method:'POST',body:JSON.stringify({phaseId,profile})});$('profile-status').textContent=result.simulationOnly?'SIMULATED; NOT DEVICE NVS':'Device confirmed save';showError('');await loadConfig();await refresh();}
    catch(e){showError(`Profile was not confirmed as saved: ${e.message}`);}
  });
  $('experiment-form').addEventListener('submit',async(e)=>{e.preventDefault();const form=new FormData(e.currentTarget);const obj=Object.fromEntries(form.entries());obj.phase=lastState?.phase||'';
    try{const result=await requestJson('/api/experiment',{method:'POST',body:JSON.stringify(obj)});$('experiment-result').textContent=result.message||'Saved';e.currentTarget.reset();}
    catch(err){$('experiment-result').textContent=err.message;}
  });
  $('chart-window').addEventListener('change',()=>{drawChart('temp-chart',history.air,'°C');drawChart('rh-chart',history.rh,'%');});
  loadConfig().then(refresh);setInterval(refresh,4000);window.addEventListener('resize',()=>{drawChart('temp-chart',history.air,'°C');drawChart('rh-chart',history.rh,'%');});
})();
