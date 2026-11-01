import dgram from 'node:dgram';
import crypto from 'node:crypto';
// getStatus is a short, challenge-bound reachability proof. getInfo embeds all
// bots and can exceed the path MTU, causing false negatives on fragmented UDP.
export function udpProof(ip,port,timeout=2200){return new Promise((resolve,reject)=>{
 const socket=dgram.createSocket('udp4'),challenge=crypto.randomBytes(8).toString('hex');let done=false;
 const finish=error=>{if(done)return;done=true;clearTimeout(timer);socket.close();error?reject(error):resolve();};
 const timer=setTimeout(()=>finish(Error('Le port UDP ne répond pas depuis Internet.')),timeout);
 socket.once('error',finish);
 socket.on('message',(packet,remote)=>{
  if(remote.address!==ip||remote.port!==port||packet.length>8192||!packet.subarray(0,4).equals(Buffer.from([255,255,255,255])))return;
  const text=packet.subarray(4).toString();
  if(!/^statusResponse[ \n]/.test(text))return;
  const first=text.slice(14).trim().split('\n')[0],fields=first.split('\\');let echoed='';
  for(let i=1;i+1<fields.length;i+=2)if(fields[i]==='challenge')echoed=fields[i+1];
  if(echoed!==challenge)return;finish();
 });
 socket.send(Buffer.concat([Buffer.from([255,255,255,255]),Buffer.from('getstatus '+challenge)]),port,ip,error=>{if(error)finish(error);});
});}
