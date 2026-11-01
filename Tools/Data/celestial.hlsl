// Shared camera direction has no positional parallax. Existing cloud alpha
// masks all bodies, so stars and the moon cannot draw in front of those clouds.
float3 d=normalize(-View);
float horizon=smoothstep(-.012,.025,d.z);
float visibility=pow(1-saturate(CloudMask),2)*horizon;
float3 s=normalize(SunDir),m=normalize(MoonDir);
float sd=length(d-s),md=length(d-m);
float saa=max(fwidth(sd),.00008),maa=max(fwidth(md),.00008);
float sun=1-smoothstep(.0056-saa,.0056+saa,sd);
float3 right=normalize(cross(float3(0,0,1),m));
float3 up=cross(m,right);
float2 q=float2(dot(d,right),dot(d,up))/.0087;
float disk=1-smoothstep(.0087-maa,.0087+maa,md);
// Broad asymmetric maria remain readable at normal FOV; avoid high-frequency
// stripes and strong sphere shading on the fixed full-moon face.
float maria= .65*exp(-dot((q-float2(-.25,.22))*float2(1,1.4),(q-float2(-.25,.22))*float2(1,1.4))*5)
 +.52*exp(-dot(q-float2(.30,.34),q-float2(.30,.34))*17)
 +.38*exp(-dot(q-float2(-.47,-.27),q-float2(-.47,-.27))*24)
 +.22*exp(-dot(q-float2(.1,-.1),q-float2(.1,-.1))*28);
float fine=.018*sin(q.x*13+sin(q.y*9))+.014*sin(q.y*19+q.x*7);
float craters=0;float footprint=max(length(fwidth(q))*.45,.018);
[unroll] for(int k=0;k<11;k++)
{
 float2 c=.76*sin(float2(k*13.17+1,k*7.31+4));
 float r=length(q-c),size=.045+.025*(k%4);
 float rim=exp(-pow((r-size)/footprint,2));
 float bowl=1-smoothstep(max(0,size-footprint),size+footprint,r);
 craters+=.035*rim-.06*bowl;
}
float relief=clamp(.90-.66*saturate(maria)+fine+craters,.24,.92);
relief*=lerp(.82,1,sqrt(saturate(1-dot(q,q))));
float moonHalo=exp(-md*md/.0002)*.0012;
float cs=cos(Spin),sn=sin(Spin);
float3 starDirection=float3(d.x*cs-d.y*sn,d.x*sn+d.y*cs,d.z);
// Cube-face placement avoids latitude/longitude seams and polar clustering.
float3 ad=abs(starDirection);float2 plane;float face,sgn;
if(ad.z>=ad.x&&ad.z>=ad.y){sgn=sign(starDirection.z);plane=starDirection.xy/ad.z;face=sgn>0?0:1;}
else if(ad.x>=ad.y){sgn=sign(starDirection.x);plane=starDirection.yz/ad.x;face=sgn>0?2:3;}
else {sgn=sign(starDirection.y);plane=starDirection.xz/ad.y;face=sgn>0?4:5;}
float2 p=(plane*.5+.5)*128,cell=floor(p),seed=cell+face*137;
float hash=frac(sin(dot(seed,float2(127.1,311.7)))*43758.5453);
float2 center=.2+.6*frac(sin(float2(dot(seed,float2(269.5,183.3)),dot(seed,float2(419.2,371.9))))*43758.5453);
float2 cplane=(cell+center)/128*2-1;
float3 starCenter;
if(face<2)starCenter=float3(cplane,sgn);
else if(face<4)starCenter=float3(sgn,cplane);
else starCenter=float3(cplane.x,sgn,cplane.y);
starCenter=normalize(starCenter);
float rank=pow(frac(hash*233),4);
float radius=lerp(.00023,.0008,rank);
float aa=max(length(fwidth(starDirection))*.45,.0001);
float star=(1-smoothstep(radius,radius+aa,length(starDirection-starCenter)))*step(.995,hash);
star*=saturate(radius/aa);
float twinkle=.98+.02*sin(Time*.65+hash*200);
float3 starTint=lerp(float3(.72,.83,1),float3(1,.9,.75),frac(hash*113));
float sunRadiance=lerp(1.8,12,smoothstep(0,.45,s.z));
float3 color=SunColor*(sun*sunRadiance+exp(-sd*sd/.00016)*.025)*SunAmount;
color+=float3(.94,.95,1)*(disk*relief*.17+moonHalo)*MoonAmount;
color+=starTint*star*(.02+.13*rank)*twinkle*StarAmount*smoothstep(.10,.40,d.z)*lerp(1,.12*(1-smoothstep(.035,.30,md))+smoothstep(.035,.30,md),MoonAmount);
// The old sky reached zero on both day and night branches at the horizon.
// Bridge that visual gap without adding a second world illumination source.
float towardSun=pow(saturate(dot(normalize(d.xy+float2(.00001,0)),normalize(s.xy+float2(.00001,0)))),4);
float low=exp(-max(0,d.z)*7);
float3 twilight=lerp(float3(.004,.012,.035),lerp(float3(.045,.055,.08),float3(.14,.06,.026),towardSun),low);
twilight=lerp(twilight,float3(.038,.043,.05),saturate(CloudMask)*.65);
return color*visibility+twilight*TwilightAmount*smoothstep(-.06,.025,d.z);
