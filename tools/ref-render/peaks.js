// Independent partial locator: fine Goertzel scan over a long steady window.
const fs=require('fs');
function readWav(p){const b=fs.readFileSync(p);let off=12,fmt,data;
 while(off<b.length-8){const id=b.toString('ascii',off,off+4),sz=b.readUInt32LE(off+4);
  if(id==='fmt ')fmt={ch:b.readUInt16LE(off+10),sr:b.readUInt32LE(off+12)};
  if(id==='data')data={off:off+8,sz};off+=8+sz+(sz&1);}
 const n=data.sz/2/fmt.ch,x=new Float64Array(n);
 for(let i=0;i<n;i++){let s=0;for(let c=0;c<fmt.ch;c++)s+=b.readInt16LE(data.off+(i*fmt.ch+c)*2);x[i]=s/fmt.ch/32768;}
 return {x,sr:fmt.sr};}
function goertzel(x,f,sr){const w=2*Math.PI*f/sr,c=2*Math.cos(w);let s1=0,s2=0;
 for(let i=0;i<x.length;i++){const s=x[i]+c*s1-s2;s2=s1;s1=s;}
 return Math.sqrt(Math.max(0,s1*s1+s2*s2-c*s1*s2));}
const {x,sr}=readWav(process.argv[2]);
const t0=Number(process.argv[3]), t1=Number(process.argv[4]), f0=220;
const seg=x.subarray(Math.floor(t0*sr),Math.floor(t1*sr));
// hann
const w=new Float64Array(seg.length);
for(let i=0;i<seg.length;i++) w[i]=seg[i]*(0.5-0.5*Math.cos(2*Math.PI*i/seg.length));
console.log(`${process.argv[2].split('/').pop()}  ${t0}-${t1}s  (${(seg.length/sr).toFixed(1)}s window, ${(sr/seg.length).toFixed(2)} Hz bins)`);
console.log(' n    ideal      peak     cents   level');
let ref=null;
for(const n of [1,2,3,4,5,6,8,10,12]){
  const ideal=n*f0; let best=0,bestf=0;
  const span=ideal*0.06;                          // +-100 cents, so a real shift cannot escape
  for(let f=ideal-span;f<=ideal+span;f+=0.05){const m=goertzel(w,f,sr); if(m>best){best=m;bestf=f;}}
  if(n===1) ref=best;
  const cents=1200*Math.log2(bestf/ideal);
  console.log(` h${String(n).padEnd(2)} ${ideal.toFixed(1).padStart(7)} ${bestf.toFixed(2).padStart(9)} ${cents>=0?'+':''}${cents.toFixed(1).padStart(6)}  ${(20*Math.log10(best/ref)).toFixed(1).padStart(6)} dB`);
}
