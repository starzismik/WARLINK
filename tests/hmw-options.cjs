const fs=require('fs'),path=require('path'),cp=require('child_process'),dgram=require('dgram');
const work=path.resolve('dev'),dir=path.join(work,'options-integration'),game='D:/SteamLibrary/steamapps/common/Call of Duty Modern Warfare Remastered';
fs.mkdirSync(path.join(dir,'warlink/data/server-assets'),{recursive:true});
fs.writeFileSync(path.join(dir,'warlink/portable.marker'),'');
fs.copyFileSync(path.resolve('build/bin/x64/Release/WARLINK.exe'),path.join(dir,'WARLINK.exe'));
let script=fs.readFileSync(path.resolve('server-assets/warlink_bots.gsc'),'utf8');script=script.replace('level thread warlink_manage_bots();','level thread warlink_manage_bots();\n    level thread warlink_test_hud();');script+='\nwarlink_test_hud() { wait 12; foreach(player in level.players) { if(isBot(player)) { setdvar("warlink_message_position","center"); player warlink_display_message("WARLINK TEST CENTER"); wait 2; setdvar("warlink_message_position","left"); player warlink_display_message("WARLINK TEST LEFT"); LogPrint("WARLINK_HUD_TEST_PASSED\\n"); return; } } }\n';script=script.replace('level thread warlink_connections();','level thread warlink_connections();\n    level thread warlink_balance_probe();');script+='\nwarlink_balance_probe() { wait 18; foreach(player in level.players) { if(isBot(player)) { player maps\\mp\\gametypes\\_menus::addtoteam("allies"); player maps\\mp\\gametypes\\_menus::endrespawnnotify(); } } wait 2; warlink_balance_bots(); LogPrint("WARLINK_BALANCE_PROBE_PASSED\\n"); }\n';fs.writeFileSync(path.join(dir,'warlink/data/server-assets/warlink_bots.gsc'),script);
let seq=0;
function api(request){const file=path.join(dir,`request-${++seq}.json`);fs.writeFileSync(file,JSON.stringify(request));const p=cp.spawnSync(path.join(dir,'WARLINK.exe'),['-hmw-host-tool',file],{cwd:dir,windowsHide:true,timeout:15000});const target=file+'.result.json';if(!fs.existsSync(target))throw Error(`native tool failed ${p.status} ${p.error||''}`);const r=JSON.parse(fs.readFileSync(target));if(!r.ok)throw Error(r.error);return r;}
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{

 const id='validation';
 try{api({action:'stop',id});}catch{}
 api({action:'save',id,profile:{name:'WARLINK Validation',gamePath:game,port:27020,access:'local',players:4,bots:2,difficulty:'regular',mode:'war',botWeapons:'sniper',botBalance:true,rotation:['mp_rust'],scoreLimit:0,timeLimit:0,health:100,killstreaks:true,voting:true}});
 fs.writeFileSync(path.join(game,'hmw-mod/logs/warlink_validation.log'),'');
 api({action:'start',id});console.log('Native manager started the validation server on 127.0.0.1:27020');
 let state;
 for(let i=0;i<150;i++){
  state=api({action:'status',id});
  fs.writeFileSync(path.join(dir,'latest-status.json'),JSON.stringify(state,null,2));
  if(!state.running)throw Error('Server exited: '+state.log.slice(-4000));
  if(i%10===0)console.log('Loading…',i*2,'seconds',state.log.slice(-200).replace(/\r?\n/g,' | '));
  if(state.ready&&/\\bots\\2/.test(state.info)){console.log('SERVER_READY_WITH_BOTS',state.info);break;}
  if(i===149)throw Error('Server did not report live bots: '+state.info+'\n'+state.log.slice(-5000));
  await sleep(2000);
 }
 await sleep(13000);console.log('RCON_STATUS',api({action:'rcon',id,command:'status'}).output.slice(0,2500));
 await sleep(1200);
 console.log('RCON_DIFFICULTY',api({action:'rcon',id,command:'set warlink_bot_difficulty veteran; warlink_bot_difficulty'}).output.slice(0,700));
 await sleep(1200);api({action:'rcon',id,command:'set warlink_bot_count 1'});
 let reduced;for(let i=0;i<12;i++){await sleep(1000);reduced=api({action:'status',id});if(/\\bots\\1/.test(reduced.info))break;}
 if(!/\\bots\\1/.test(reduced.info))throw Error('Bot count live change failed: '+reduced.info+' '+api({action:'rcon',id,command:'warlink_bot_count'}).output);
 for(const mode of ['none','knife','mixed','sniper']){await sleep(1500);api({action:'rcon',id,command:'set warlink_bot_weapons '+mode});await sleep(5000);const st=api({action:'status',id});if(!st.ready)throw Error('Not ready after loadout '+mode);console.log('LOADOUT_MODE',mode);}
 const gameLog=fs.readFileSync(path.join(game,'hmw-mod/logs/warlink_validation.log'),'utf8');if(!gameLog.includes('WARLINK_BOT_TEAM_BALANCED'))throw Error('Bot transfer not exercised');for(const mode of ['sniper','none','knife'])if(!gameLog.includes('WARLINK_LOADOUT_APPLIED '+mode))throw Error('No runtime acknowledgement for '+mode);if(!gameLog.includes('WARLINK_HUD_TEST_PASSED'))throw Error('HUD message runtime test failed');if(!/WARLINK_BOT_DIFFICULTY veteran\r?\n/.test(gameLog))throw Error('Difficulty not acknowledged by bot script');
 api({action:'rcon',id,command:'map mp_terminal'});
 let mapChanged=false;
 for(let i=0;i<80;i++){const r=api({action:'status',id});if(!r.running)throw Error('Server exited during map change');if(r.ready&&/\\mapname\\mp_terminal/.test(r.info)&&/\\bots\\1/.test(r.info)){mapChanged=true;break;}await sleep(1500);}
 if(!mapChanged)throw Error('Live map change did not complete with a bot');
 console.log('LIVE_MAP_CHANGE_PASSED mp_terminal');
 fs.writeFileSync(path.join(dir,'passed.json'),JSON.stringify({serverReady:true,bots:2,liveBotCount:true,rcon:true,difficulty:true,mapChange:true,maps:['mp_rust','mp_terminal'],version:'1.6.7'},null,2));
 api({action:'stop',id});const stopped=api({action:'status',id});if(stopped.running)throw Error('Server did not stop');console.log('ALL HMW INTEGRATION CHECKS PASSED');
})().catch(e=>{try{api({action:'stop',id:'validation'});}catch{}console.error(e.message);process.exitCode=1;}).finally(()=>fs.copyFileSync(path.resolve('server-assets/warlink_bots.gsc'),path.join(game,'user_scripts/mp/warlink_bots.gsc')));
