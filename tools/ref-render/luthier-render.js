const fs=require('fs');
const PHYS=fs.readFileSync('phys.js','utf8');
const scope=new Function(PHYS+'\nreturn {StringSim,MATERIALS,Engine};')();
global.StringSim=scope.StringSim; global.MATERIALS=scope.MATERIALS;
eval(fs.readFileSync('tune.js','utf8'));
const SR=44100, f0=Number(process.argv[2]||220), mat=process.argv[3]||'gut';
const vb=Number(process.argv[4]||0.20), fb=Number(process.argv[5]||1050), pos=0.14;
const t=tuneString(SR,f0,mat);
console.error(`tuned ${mat} f0=${f0} -> gamma=${t.gamma.toFixed(1)} N=${t.N} nodes, measured ${t.measured.toFixed(2)}Hz (${t.cents.toFixed(2)} cents)`);
const M=MATERIALS[mat];
const e=new scope.Engine(SR);
e.setConfig([{f0,gamma:t.gamma,kappa:M.kappa,sig0:M.sig0,sig1:M.sig1}]);
if(process.env.NOBODY) e.msg({t:'body',on:false});
const secs=7, N=SR*secs, buf=new Float32Array(N), blk=new Float32Array(128);
let bowOn=false;
for(let n=0;n<N;n+=128){
  const tt=n/SR;
  if(!bowOn && tt>=0.5){ e.msg({t:'bow',id:0,s:0,pos,vb,fb,on:true}); bowOn=true; }
  if(bowOn && tt>=6.0){ e.msg({t:'bow',id:0,on:false}); bowOn=false; }
  e.block(blk); buf.set(blk.subarray(0,Math.min(128,N-n)),n);
}
// 16-bit wav
const hdr=Buffer.alloc(44); const bytes=N*2;
hdr.write('RIFF',0); hdr.writeUInt32LE(36+bytes,4); hdr.write('WAVE',8);
hdr.write('fmt ',12); hdr.writeUInt32LE(16,16); hdr.writeUInt16LE(1,20); hdr.writeUInt16LE(1,22);
hdr.writeUInt32LE(SR,24); hdr.writeUInt32LE(SR*2,28); hdr.writeUInt16LE(2,32); hdr.writeUInt16LE(16,34);
hdr.write('data',36); hdr.writeUInt32LE(bytes,40);
const pcm=Buffer.alloc(bytes);
let pk=0; for(let i=0;i<N;i++) pk=Math.max(pk,Math.abs(buf[i]));
for(let i=0;i<N;i++) pcm.writeInt16LE(Math.max(-32768,Math.min(32767,Math.round(buf[i]*32767))),i*2);
fs.writeFileSync(process.argv[6]||'luthier.wav',Buffer.concat([hdr,pcm]));
console.error(`peak ${(20*Math.log10(pk)).toFixed(1)} dBFS -> ${process.argv[6]||'luthier.wav'}`);
