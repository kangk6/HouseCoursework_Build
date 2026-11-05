"""Periodic trajectory fitting followed by connected limb reconstruction.

Fit the complete cycle rather than rushing the last six keys back to key zero.
Use one cycle-wide safety margin so collision/reach correction cannot toggle
on and off at individual keys. All calculations target the existing Harper suit.
"""
from gltf_math import *
from arm_clearance import SuitClearance

def unit(v):return v/max(np.linalg.norm(v),1e-9)
def between(a,b):
 a,b=unit(a),unit(b);v=np.cross(a,b);c=np.clip(a@b,-1,1)
 if c<-.99999:return R.from_rotvec(unit(np.cross(a,[1,0,0] if abs(a[0])<.8 else [0,1,0]))*np.pi)
 return R.from_quat(np.r_[v,1+c]/np.linalg.norm(np.r_[v,1+c]))

def periodic(values,harmonics=6):
 """Least-squares Fourier curve sampled at original phases; exact endpoint."""
 values=np.asarray(values);n=len(values)-1;t=np.arange(n)/n
 basis=np.column_stack([np.ones(n)]+[f(2*np.pi*k*t) for k in range(1,harmonics+1) for f in (np.sin,np.cos)])
 # Share the seam residual across the whole cycle before the periodic fit.
 v=values[:-1]-t[:,None]*(values[-1]-values[0])
 fit=basis@np.linalg.lstsq(basis,v,rcond=None)[0]
 return np.concatenate([fit,fit[:1]])

def finish_cycle(g,ref,tracks):
 names={n.get('name'):i for i,n in enumerate(g.nodes)};rest=g.local();rw=g.world(rest)
 C=np.array([[1,0,0],[0,0,1],[0,1,0]]);wrap=g.nodes[69]['scale'][0];suit=SuitClearance(g)
 result=copy.deepcopy(tracks)
 for bone in tracks[0]:
  for key in ['translation','scale']:
   fitted=periodic([f[bone][key] for f in tracks])
   for f,v in zip(result,fitted):f[bone][key]=v.tolist()
  rotations=R.from_quat([f[bone]['rotation'] for f in tracks]);base=rotations[0]
  rv=(base.inv()*rotations).as_rotvec();fitted=periodic(rv)
  for f,v in zip(result,(base*R.from_rotvec(fitted)).as_quat()):f[bone]['rotation']=v.tolist()
 poses=[]
 for frame in result:
  lp,lq,ls=loc=g.local()
  for n,v in frame.items():
   if n not in names or names[n]==68:continue
   i=names[n];factor=wrap if i in [1,3,55] else 1
   lp[i]=C@np.array(v['translation'])/100/factor;lq[i]=R.from_matrix(C@R.from_quat(v['rotation']).as_matrix()@C);ls[i]=np.array(v['scale'])/factor
  poses.append(loc)
 def position(loc,n):return g.world(loc)[names[n]][:3,3]
 def update(loc,n,position=None,rotation=None):
  i=names[n];world=g.world(loc);par=g.parent.get(i);pm=world[par] if par is not None else np.eye(4)
  if position is not None:loc[0][i]=(np.linalg.inv(pm)@np.r_[position,1])[:3]
  if rotation is not None:loc[1][i]=R.from_matrix(pm[:3,:3]).inv()*rotation
 def orient(loc,n,child,direction):
  update(loc,n,rotation=between(rw[names[child]][:3,3]-rw[names[n]][:3,3],direction)*R.from_matrix(rw[names[n]][:3,:3]))
 def lengths(a,b,c):return np.linalg.norm(rw[names[b]][:3,3]-rw[names[a]][:3,3]),np.linalg.norm(rw[names[c]][:3,3]-rw[names[b]][:3,3])
 def knee_or_elbow(origin,goal,a,b,pole):
  delta=goal-origin;d=np.linalg.norm(delta);u=unit(delta);pole=unit(pole-u*(pole@u));x=(a*a-b*b+d*d)/(2*d)
  return origin+u*x+pole*np.sqrt(max(0,a*a-x*x))
 # Preserve flat-ground contact after fitting; one uniform lift cannot create
 # an individual-frame clamp. Heel and toe are the same support points as before.
 lift=0
 for loc in poses:
  world=g.world(loc)
  for s in ['l','r']:
   foot=world[names['foot_'+s]];restfoot=rw[names['foot_'+s]];delta=R.from_matrix(foot[:3,:3])*R.from_matrix(restfoot[:3,:3]).inv()
   heel=np.array([0,-restfoot[1,3],-.035]);toe=rw[names['ball_'+s]][:3,3]-restfoot[:3,3];toe[1]-=rw[names['ball_'+s]][1,3]
   lift=max(lift,-foot[1,3]-min(delta.apply(heel)[1],delta.apply(toe)[1]))
 drop=0
 for loc in poses:
  for s in ['l','r']:
   goal=position(loc,'foot_'+s).copy();goal[1]+=max(0,lift);update(loc,'foot_'+s,position=goal)
   hip=position(loc,'thigh_'+s);a,b=lengths('thigh_'+s,'calf_'+s,'foot_'+s);flat=np.linalg.norm((goal-hip)[[0,2]])
   drop=max(drop,hip[1]-goal[1]-np.sqrt(max(.001,(a+b-.008)**2-flat*flat)))
 for loc in poses:
  pel=position(loc,'pelvis').copy();pel[1]-=max(0,drop);update(loc,'pelvis',position=pel)
  for s in ['l','r']:
   hip=position(loc,'thigh_'+s);goal=position(loc,'foot_'+s);a,b=lengths('thigh_'+s,'calf_'+s,'foot_'+s)
   knee=knee_or_elbow(hip,goal,a,b,np.array([.035 if s=='l' else -.035,0,1]))
   orient(loc,'thigh_'+s,'calf_'+s,knee-hip);orient(loc,'calf_'+s,'foot_'+s,goal-knee)
 # Fit first, then determine a cycle-wide suit margin. Re-solve all phases
 # together instead of activating a different correction branch at each frame.
 arm_bases={s:[] for s in ['l','r']}
 for loc in poses:
  for s in ['l','r']:arm_bases[s].append((position(loc,'upperarm_'+s),position(loc,'lowerarm_'+s),position(loc,'hand_'+s)))
 shifts={s:0. for s in ['l','r']}
 for iteration in range(5):
  for s in ['l','r']:
   sign=1 if s=='l' else -1;a,b=lengths('upperarm_'+s,'lowerarm_'+s,'hand_'+s);raise_y=0
   for shoulder,elbow,goal0 in arm_bases[s]:
    goal=goal0+np.array([sign*shifts[s],0,0]);flat=np.linalg.norm((goal-shoulder)[[0,2]])
    assert flat<a+b-.008,'Arm clearance target exceeds reach'
    raise_y=max(raise_y,shoulder[1]-np.sqrt(max(.001,(a+b-.008)**2-flat*flat))-goal[1])
   for loc,(shoulder,elbow0,goal0) in zip(poses,arm_bases[s]):
    goal=goal0+np.array([sign*shifts[s],raise_y,0]);pole=np.array([sign*.35,0,-1.])
    elbow=knee_or_elbow(shoulder,goal,a,b,pole);orient(loc,'upperarm_'+s,'lowerarm_'+s,elbow-shoulder);orient(loc,'lowerarm_'+s,'hand_'+s,goal-elbow)
    forward=unit(goal-elbow);wrist=R.from_rotvec(forward*sign*np.radians(8))*between(rw[names['hand_'+s]][:3,3]-rw[names['lowerarm_'+s]][:3,3],forward)*R.from_matrix(rw[names['hand_'+s]][:3,:3]);update(loc,'hand_'+s,rotation=wrist)
  worst={s:0. for s in ['l','r']}
  for loc in poses:
   world=g.world(loc);cloth=suit.skin(suit.cloth,world)
   for s in ['l','r']:worst[s]=max(worst[s],suit.penetration(suit.skin(suit.hands[s],world),cloth,s,margin=.008))
  if max(worst.values())<.0005:break
  for s in ['l','r']:shifts[s]+=worst[s]+(.002 if worst[s]>0 else 0)
 else:raise AssertionError('Cycle-wide suit margin did not converge')
 for frame,loc in zip(result,poses):
  lp,lq,ls=loc
  for n in frame:
   if n not in names or names[n]==68:continue
   i=names[n];factor=wrap if i in [1,3,55] else 1
   frame[n]={'translation':(C@lp[i]*100*factor).tolist(),'rotation':R.from_matrix(C@lq[i].as_matrix()@C).as_quat().tolist(),'scale':(ls[i]*factor).tolist()}
 result[-1]=copy.deepcopy(result[0])
 print('periodic fit: foot lift cm',lift*100,'pelvis margin cm',drop*100,'arm offsets cm',{s:v*100 for s,v in shifts.items()})
 return result
