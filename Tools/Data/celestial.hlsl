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
float2 q=float2(dot(d,right),dot(d,up))/.0113;
float disk=1-smoothstep(.0113-maa,.0113+maa,md);
float maria=exp(-dot(q-float2(-.25,.15),q-float2(-.25,.15))*9)
 +.7*exp(-dot(q-float2(.22,.42),q-float2(.22,.42))*18)
 +.5*exp(-dot(q-float2(-.4,-.3),q-float2(-.4,-.3))*25);
float fine=.04*sin(q.x*75+sin(q.y*32))+.025*sin(q.y*103+q.x*48);
float craters=0;
[unroll] for(int k=0;k<7;k++)
{
 float2 c=.72*sin(float2(k*13.17+1,k*7.31+4));
 float r=length(q-c),size=.035+.014*(k%3);
 craters+=.10*exp(-pow((r-size)/.014,2))-.075*exp(-pow(r/size,2));
}
float relief=saturate(.88-.28*maria+fine+craters)*lerp(.5,1,sqrt(saturate(1-dot(q,q))));
float moonHalo=exp(-md*md/ .0009)*.003;
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
float radius=lerp(.00025,.00065,frac(hash*71));
float aa=max(length(fwidth(starDirection))*.45,.0001);
float star=(1-smoothstep(radius,radius+aa,length(starDirection-starCenter)))*step(.993,hash);
star*=saturate(radius/aa);
float twinkle=.94+.06*sin(Time*.9+hash*200);
float3 starTint=lerp(float3(.72,.83,1),float3(1,.9,.75),frac(hash*113));
float3 color=SunColor*(sun*18+exp(-sd*sd/.0006)*.08)*SunAmount;
color+=float3(.86,.9,1)*(disk*relief*.14+moonHalo)*MoonAmount;
color+=starTint*star*(.025+.075*frac(hash*233))*twinkle*StarAmount*smoothstep(.025,.18,d.z);
// The old sky reached zero on both day and night branches at the horizon.
// Bridge that visual gap without adding a second world illumination source.
float towardSun=pow(saturate(dot(normalize(d.xy+float2(.00001,0)),normalize(s.xy+float2(.00001,0)))),4);
float low=exp(-max(0,d.z)*5);
float3 twilight=lerp(float3(.004,.012,.035),lerp(float3(.08,.075,.08),float3(.18,.065,.025),towardSun),low);
twilight=lerp(twilight,float3(.038,.043,.05),saturate(CloudMask)*.65);
return color*visibility+twilight*TwilightAmount*smoothstep(-.06,.025,d.z);
