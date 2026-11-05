"""Two planted-foot steps for the fixed evacuation startup quarter-turn.

Actor yaw must follow the same cubic curve over one second. Feet are solved
in world space, then transformed back into the actor frame. Not root motion.
"""
from gltf_math import *

folder=Path(__file__).parent
g=GLB(sys.argv[1]); names={n.get('name'):i for i,n in enumerate(g.nodes)}
rest=g.local(); rw=g.world(rest); neutral=g.local(0); nw=g.world(neutral)
ref=json.loads((folder/'ue_ref.json').read_text())
C=np.array([[1,0,0],[0,0,1],[0,1,0]])
wrap=g.nodes[69]['scale'][0]
def unit(v):return v/max(np.linalg.norm(v),1e-9)
def smooth(t):
 t=np.clip(t,0,1);return t*t*(3-2*t)
def between(a,b):
 a,b=unit(a),unit(b);v=np.cross(a,b);c=np.clip(a@b,-1,1)
 if c<-.99999:return R.from_rotvec(unit(np.cross(a,[1,0,0] if abs(a[0])<.8 else [0,1,0]))*np.pi)
 return R.from_quat(np.r_[v,1+c]/np.linalg.norm(np.r_[v,1+c]))

for sign,label in [(1,'Positive90'),(-1,'Negative90')]:
 out=[]
 for frame in range(61):
  t=frame/60;lp,lq,ls=loc=copy.deepcopy(neutral);ls[names['held_accessory']]=np.zeros(3)
  actor=R.from_euler('Y',-sign*90*smooth(t),degrees=True)
  def pos(n):return g.world(loc)[names[n]][:3,3]
  def update(n,position=None,rotation=None):
   i=names[n];world=g.world(loc);par=g.parent.get(i);pm=world[par] if par is not None else np.eye(4)
   if position is not None:lp[i]=(np.linalg.inv(pm)@np.r_[position,1])[:3]
   if rotation is not None:lq[i]=R.from_matrix(pm[:3,:3]).inv()*rotation
  def orient(n,child,direction):
   v=rw[names[child]][:3,3]-rw[names[n]][:3,3]
   update(n,rotation=between(v,direction)*R.from_matrix(rw[names[n]][:3,:3]))
  goals={};rotations={}
  for s in ['l','r']:
   first=(s=='r')==(sign>0);start,end=(0,.56) if first else (.44,1)
   phase=float(np.clip((t-start)/(end-start),0,1));f=smooth(phase)
   foot=nw[names['foot_'+s]][:3,3];final=R.from_euler('Y',-sign*90,degrees=True).apply(foot)
   worldfoot=foot*(1-f)+final*f;worldfoot[1]+=.045*np.sin(np.pi*phase)**2
   goals[s]=actor.inv().apply(worldfoot)
   rotations[s]=actor.inv()*R.from_euler('Y',-sign*90*f,degrees=True)*R.from_matrix(nw[names['foot_'+s]][:3,:3])
  # Give the knees room while the stance feet are at different headings.
  pelvis=pos('pelvis');pelvis[1]-=.018*np.sin(np.pi*t)**2;update('pelvis',position=pelvis)
  drop=0
  for s in ['l','r']:
   hip=pos('thigh_'+s);goal=goals[s]
   a=np.linalg.norm(rw[names['calf_'+s]][:3,3]-rw[names['thigh_'+s]][:3,3]);b=np.linalg.norm(rw[names['foot_'+s]][:3,3]-rw[names['calf_'+s]][:3,3])
   flat=np.linalg.norm((goal-hip)[[0,2]]);drop=max(drop,hip[1]-goal[1]-np.sqrt(max(.001,(a+b-.002)**2-flat*flat)))
  pelvis[1]-=max(0,drop);update('pelvis',position=pelvis)
  for s in ['l','r']:
   hip=pos('thigh_'+s);goal=goals[s];a=np.linalg.norm(rw[names['calf_'+s]][:3,3]-rw[names['thigh_'+s]][:3,3]);b=np.linalg.norm(rw[names['foot_'+s]][:3,3]-rw[names['calf_'+s]][:3,3])
   d=np.linalg.norm(goal-hip);u=unit(goal-hip);pole=unit(np.array([0,0,1])-u*u[2]);x=(a*a-b*b+d*d)/(2*d);knee=hip+u*x+pole*np.sqrt(max(0,a*a-x*x))
   orient('thigh_'+s,'calf_'+s,knee-hip);orient('calf_'+s,'foot_'+s,goal-knee);update('foot_'+s,position=goal,rotation=rotations[s])
  vals={}
  for i in g.d['skins'][0]['joints']:
   n=g.nodes[i].get('name')
   if n not in ref:continue
   if i==68:vals[n]=ref[n];continue
   vals[n]={'translation':(C@lp[i]*100*(wrap if i in [1,3,55] else 1)).tolist(),'rotation':R.from_matrix(C@lq[i].as_matrix()@C).as_quat().tolist(),'scale':(ls[i]*(wrap if i in [1,3,55] else 1)).tolist()}
  out.append(vals)
 (folder/('turn_'+label+'.json')).write_text(json.dumps({'fps':60,'frames':out,'source':'Authored two-step quarter turn from Harper idle; actor cubic yaw synchronized externally'}),encoding='utf-8')
 print(label,len(out))
