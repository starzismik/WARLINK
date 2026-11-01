import http from 'node:http';
let updateCache=JSON.parse(fsReadBaseline());
function fsReadBaseline(){return '{"version":"1.0.0","released":false,"mandatory":true,"downloadUrl":""}';}
let lastUpdateCheck=0;
async function updateFeed(){if(Date.now()-lastUpdateCheck>60000){lastUpdateCheck=Date.now();try{const r=await fetch('https://raw.githubusercontent.com/starzismik/WARLINK/main/update.json',{signal:AbortSignal.timeout(5000)});if(r.ok){const d=await r.json();if(d.released===true&&/^\d+\.\d+\.\d+$/.test(d.version)&&String(d.downloadUrl).startsWith('https://github.com/starzismik/WARLINK/releases/download/')){updateCache=d;fs.writeFileSync(path.join(dir,'required-update.json'),JSON.stringify(d));}}}catch{}}return updateCache;}

import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {udpProof as udp} from './udp-probe.mjs';
import {isIP} from 'node:net';
export function publicIPv4(ip){if(isIP(ip)!==4)return false;const [a,b]=ip.split('.').map(Number);return !(a===0||a===10||a===127||a>=224||(a===169&&b===254)||(a===172&&b>=16&&b<=31)||(a===192&&b===168)||(a===100&&b>=64&&b<=127)||(a===198&&(b===18||b===19)));}
const dir=process.env.DATA_DIR||'./data';fs.mkdirSync(dir,{recursive:true});const file=path.join(dir,'servers.json');let entries={};try{entries=JSON.parse(fs.readFileSync(file));}catch(e){if(e.code!=='ENOENT')throw e;}
try{const saved=JSON.parse(fs.readFileSync(path.join(dir,'required-update.json')));if(saved.released===true)updateCache=saved;}catch{}
const persist=()=>{fs.writeFileSync(file+'.tmp',JSON.stringify(entries));fs.renameSync(file+'.tmp',file);};
const digest=s=>crypto.createHash('sha256').update(s).digest('hex');
const clean=(s,n=160)=>String(s??'').replace(/[\x00-\x1f]/g,'').slice(0,n);
async function probe(ip,port){if(!publicIPv4(ip)||!Number.isInteger(port)||port<1024||port>65535)throw Error('Adresse publique ou port invalide');await udp(ip,port);const r=await fetch(`http://${ip}:${port}/getInfo`,{signal:AbortSignal.timeout(3500),redirect:'error'});if(!r.ok)throw Error('Le port TCP ne répond pas');const text=await r.text();if(text.length>64000)throw Error('Réponse serveur trop grande');const j=JSON.parse(text);if(!String(j.gamename||'').toLowerCase().includes('hmw'))throw Error('Serveur HorizonMW requis');return j;}
function view(e){const j=e.info||{};return{id:e.id,name:clean(j.hostname||'HorizonMW'),map:clean(j.mapname,64),mode:'mp',gametype:clean(j.gametype||'dm',32),players:Math.max(0,Math.min(18,Number(j.clients)||0)),maxPlayers:Math.max(2,Math.min(18,Number(j.sv_maxclients)||18)),bots:Math.max(0,Math.min(17,Number(j.bots)||0)),locked:String(j.isPrivate)==='1',region:'EU',ping:null,community:true,verifiedAt:e.verifiedAt};}
const rates=new Map();const busy=new Set();let inFlight=0;
const server=http.createServer(async(req,res)=>{res.setHeader('Content-Type','application/json; charset=utf-8');res.setHeader('Access-Control-Allow-Origin','*');res.setHeader('Cache-Control','no-store');const send=(status,obj)=>{res.writeHead(status);res.end(JSON.stringify(obj));};try{
const url=new URL(req.url,'http://localhost');if(req.method==='GET'&&url.pathname==='/health')return send(200,{ok:true,service:'WARLINK Community',author:'STARZISMIK'});
if(req.method==='GET'&&url.pathname==='/v1/updates')return send(200,await updateFeed());
if(req.method==='GET'&&url.pathname==='/v1/servers')return send(200,{servers:Object.values(entries).filter(e=>e.verifiedAt>Date.now()-100000).map(view)});
if(req.method!=='POST'||!['/v1/publish','/v1/unpublish'].includes(url.pathname))return send(404,{error:'Introuvable'});
// Railway overwrites X-Real-IP at its trusted edge. Never accept a caller-selected host.
const source=String(req.headers['x-real-ip']||req.socket.remoteAddress||'').replace(/^::ffff:/,'');if(!publicIPv4(source))return send(400,{error:'Adresse IPv4 publique requise'});
const rate=rates.get(source)||{time:Date.now(),n:0};if(Date.now()-rate.time>60000){rate.time=Date.now();rate.n=0;}if(++rate.n>12)return send(429,{error:'Trop de demandes'});rates.set(source,rate);
let body='';for await(const chunk of req){body+=chunk;if(body.length>4096)return send(413,{error:'Requête trop grande'});}const input=JSON.parse(body);if(!/^[a-f0-9]{64}$/.test(input.token||''))return send(400,{error:'Jeton propriétaire invalide'});const port=Number(input.port),id=`${source}:${port}`,hash=digest(input.token),old=entries[id];
if(old&&old.owner!==hash)return send(403,{error:'Ce serveur appartient à un autre éditeur'});
if(url.pathname==='/v1/unpublish'){if(old){delete entries[id];persist();}return send(200,{ok:true});}
if(inFlight>=8||busy.has(id))return send(429,{error:'Vérification en cours'});busy.add(id);inFlight++;try{const info=await probe(source,port);if(!old&&!String(info.sv_motd||'').includes('[WL:'+hash+']'))return send(403,{error:'Preuve de contrôle du serveur absente'});if(!old&&Object.keys(entries).length>=1024)return send(503,{error:'Annuaire complet'});entries[id]={id,ip:source,port,owner:hash,verifiedAt:Date.now(),info:{hostname:clean(info.hostname),mapname:clean(info.mapname,64),gametype:clean(info.gametype,32),clients:info.clients,sv_maxclients:info.sv_maxclients,bots:info.bots,isPrivate:info.isPrivate}};persist();return send(200,{ok:true,server:view(entries[id])});}finally{busy.delete(id);inFlight--;}
}catch(e){send(400,{error:clean(e.message)});}});
let checking=false;setInterval(async()=>{if(checking)return;checking=true;try{for(const e of Object.values(entries)){if(busy.has(e.id))continue;try{e.info=await probe(e.ip,e.port);e.info.sv_serverkey=undefined;e.verifiedAt=Date.now();}catch{}if(Date.now()-e.verifiedAt>86400000)delete entries[e.id];}persist();for(const[k,v]of rates)if(Date.now()-v.time>120000)rates.delete(k);}finally{checking=false;}},30000).unref();
server.listen(Number(process.env.PORT)||3000,'0.0.0.0',()=>console.log('WARLINK Community ready'));
