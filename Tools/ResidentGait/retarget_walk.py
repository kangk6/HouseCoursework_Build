from gltf_math import *
import re
folder=Path(__file__).parent
g=GLB(sys.argv[1]);names={n.get('name'):i for i,n in enumerate(g.nodes)};rest=g.local();rw=g.world(rest);ref=json.loads((folder/'ue_ref.json').read_text())
def unit(v):return v/max(np.linalg.norm(v),1e-9)
def between(a,b):
 a,b=unit(a),unit(b);v=np.cross(a,b);c=np.clip(a@b,-1,1)
 if c<-.99999:return R.from_rotvec(unit(np.cross(a,[1,0,0] if abs(a[0])<.8 else [0,1,0]))*np.pi)
 return R.from_quat(np.r_[v,1+c]/np.linalg.norm(np.r_[v,1+c]))
tokens=re.findall(r'[{}]|[^\s{}]+',(folder/'cmu_07_01.bvh.txt').read_text());k=0;bn=[];parents=[];offset=[];channels=[]
def parse(parent=-1):
 global k
 typ=tokens[k];k+=1;name=tokens[k];k+=1
 if typ=='End':name=bn[parent]+'_end'
 idx=len(bn);bn.append(name);parents.append(parent);offset.append([0,0,0]);channels.append([]);assert tokens[k]=='{';k+=1
 while tokens[k]!='}':
  if tokens[k]=='OFFSET':offset[idx]=list(map(float,tokens[k+1:k+4]));k+=4
  elif tokens[k]=='CHANNELS':n=int(tokens[k+1]);channels[idx]=tokens[k+2:k+2+n];k+=n+2
  else:parse(idx)
 k+=1
 return idx
k=1;parse();assert tokens[k]=='MOTION';k+=1;assert tokens[k]=='Frames:';frames=int(tokens[k+1]);k+=2;dt=float(tokens[k+2]);k+=3
raw=np.array(list(map(float,tokens[k:]))).reshape(frames,-1);bi={n:i for i,n in enumerate(bn)}
P=np.zeros((frames,len(bn),3));Q=np.zeros((frames,len(bn),4));cursor=0
for i in range(len(bn)):
 ch=channels[i];vals=raw[:,cursor:cursor+len(ch)];cursor+=len(ch);p=np.tile(offset[i],(frames,1)).astype(float);angles=[];order=''
 for j,c in enumerate(ch):
  if c.endswith('position'):p[:,'XYZ'.index(c[0])]+=vals[:,j]
  else:order+=c[0];angles.append(vals[:,j])
 q=R.from_euler(order,np.stack(angles,axis=1),degrees=True) if order else R.from_quat(np.tile([0,0,0,1],(frames,1)))
 if parents[i]>=0:
  par=parents[i];pq=R.from_quat(Q[:,par]);P[:,i]=P[:,par]+pq.apply(p);Q[:,i]=(pq*q).as_quat()
 else:P[:,i]=p;Q[:,i]=q.as_quat()
# Heading follows travel, then source global positions are centered on the hips.
travel=P[min(frames-1,240),0]-P[30,0];travel[1]=0;align=between(travel,[0,0,1]);P=align.apply(P.reshape(-1,3)).reshape(P.shape);Q=(align*R.from_quat(Q.reshape(-1,4))).as_quat().reshape(Q.shape)
features=(P[:,[bi[x] for x in ['LeftFoot','RightFoot','LeftLeg','RightLeg']]]-P[:,0,None,:]).reshape(frames,-1)
choices=[(np.linalg.norm(features[a]-features[z]),a,z) for a in range(30,min(110,frames-180)) for z in range(a+100,min(a+170,frames))];err,start,end=min(choices)
scale=np.mean([np.linalg.norm(rw[names['calf_'+s]][:3,3]-rw[names['thigh_'+s]][:3,3])+np.linalg.norm(rw[names['foot_'+s]][:3,3]-rw[names['calf_'+s]][:3,3]) for s in ['l','r']])/np.mean([np.linalg.norm(np.array(offset[bi[x]]))+np.linalg.norm(np.array(offset[bi[y]])) for x,y in [('LeftLeg','LeftFoot'),('RightLeg','RightFoot')]])
cycle_time=(end-start)*dt;count=round(cycle_time*60)+1;indices=np.linspace(start,end,count).round().astype(int);out=[];C=np.array([[1,0,0],[0,0,1],[0,1,0]]);tops=[1,3,55];wrap=g.nodes[69]['scale'][0]
meanhip=P[start:end+1,0,1].mean();footfloor={s:np.percentile(P[start:end+1,bi[s+'Foot'],1],5) for s in ['Left','Right']};toefloor={s:np.percentile(P[start:end+1,bi[s+'ToeBase'],1],5) for s in ['Left','Right']};flatpitch={}
for side in ['Left','Right']:
 v=P[start:end+1,bi[side+'ToeBase']]-P[start:end+1,bi[side+'Foot']];h=P[start:end+1,bi[side+'Foot'],1];flatpitch[side]=np.median(np.arctan2(v[h<np.percentile(h,30),1],v[h<np.percentile(h,30),2]))
for frame in indices:
 loc=copy.deepcopy(rest);lp,lq,ls=loc;ls[names['held_accessory']]=np.zeros(3)
 def update(n,position=None,rotation=None):
  i=names[n];w=g.world(loc);par=g.parent.get(i);pm=w[par] if par is not None else np.eye(4)
  if position is not None:lp[i]=(np.linalg.inv(pm)@np.r_[position,1])[:3]
  if rotation is not None:lq[i]=R.from_matrix(pm[:3,:3]).inv()*rotation
 def pos(n):return g.world(loc)[names[n]][:3,3]
 def orient(n,direction,restchild):
  i=names[n];v=rw[names[restchild]][:3,3]-rw[i][:3,3];update(n,rotation=between(v,direction)*R.from_matrix(rw[i][:3,:3]))
 rootdelta=R.from_quat(Q[frame,0])*R.from_quat(Q[start,0]).inv();pel=rw[names['pelvis']][:3,3].copy();pel[1]-=.018;pel[1]+=scale*(P[frame,0,1]-meanhip);pel[0]+=scale*(P[frame,0,0]-P[start,0,0])*0 # root motion excluded
 update('pelvis',position=pel,rotation=rootdelta*R.from_matrix(rw[names['pelvis']][:3,:3]))
 for n,child,a,b in [('spine_01','spine_02','Spine','Spine1'),('spine_02','spine_03','Spine1','Neck1'),('spine_03','neck_01','Neck1','Head'),('neck_01','head','Neck1','Head')]:orient(n,P[frame,bi[b]]-P[frame,bi[a]],child)
 update('head',rotation=R.from_matrix(rw[names['head']][:3,:3]))
 goals={};footqs={};clear={}
 for side,s in [('Left','l'),('Right','r')]:
  rel=(P[frame,bi[side+'Foot']]-P[frame,0])*scale;goal=rw[names['foot_'+s]][:3,3].copy();goal[0]=pel[0]+rel[0];goal[2]=pel[2]+rel[2]
  fv=P[frame,bi[side+'ToeBase']]-P[frame,bi[side+'Foot']];pitch=np.arctan2(fv[1],fv[2])-flatpitch[side];footdelta=R.from_rotvec([-pitch,0,0]);footqs[s]=footdelta*R.from_matrix(rw[names['foot_'+s]][:3,:3]);clear[s]=max(0,scale*min(P[frame,bi[side+'Foot'],1]-footfloor[side],P[frame,bi[side+'ToeBase'],1]-toefloor[side]))
  ankle=rw[names['foot_'+s]][:3,3];heel=np.array([0,-ankle[1],-.035]);toe=rw[names['ball_'+s]][:3,3]-ankle;toe[1]-=rw[names['ball_'+s]][1,3];goal[1]=clear[s]-min(footdelta.apply(heel)[1],footdelta.apply(toe)[1]);goals[s]=goal
 common=min(clear.values());pel[1]-=common
 for goal in goals.values():goal[1]-=common
 update('pelvis',position=pel)
 drop=0
 for side,s in [('Left','l'),('Right','r')]:
  hip=pos('thigh_'+s);goal=goals[s];a=np.linalg.norm(rw[names['calf_'+s]][:3,3]-rw[names['thigh_'+s]][:3,3]);b=np.linalg.norm(rw[names['foot_'+s]][:3,3]-rw[names['calf_'+s]][:3,3]);flat=np.linalg.norm((goal-hip)[[0,2]]);maxy=goal[1]+np.sqrt(max(.001,(a+b-.004)**2-flat*flat));drop=max(drop,hip[1]-maxy)
 pel[1]-=max(0,drop);update('pelvis',position=pel)
 for side,s in [('Left','l'),('Right','r')]:
  for n,child,a,b in [('upperarm_'+s,'lowerarm_'+s,side+'Arm',side+'ForeArm'),('lowerarm_'+s,'hand_'+s,side+'ForeArm',side+'Hand')]:orient(n,P[frame,bi[b]]-P[frame,bi[a]],child)
  hip=pos('thigh_'+s);goal=goals[s];a=np.linalg.norm(rw[names['calf_'+s]][:3,3]-rw[names['thigh_'+s]][:3,3]);b=np.linalg.norm(rw[names['foot_'+s]][:3,3]-rw[names['calf_'+s]][:3,3]);d=np.linalg.norm(goal-hip);u=unit(goal-hip)
  pole=(P[frame,bi[side+'Leg']]-P[frame,bi[side+'UpLeg']])*scale;pole=unit(pole-u*(pole@u));x=(a*a-b*b+d*d)/(2*d);knee=hip+u*x+pole*np.sqrt(max(0,a*a-x*x))
  orient('thigh_'+s,knee-hip,'calf_'+s);v=rw[names['foot_'+s]][:3,3]-rw[names['calf_'+s]][:3,3];update('calf_'+s,rotation=between(v,goal-knee)*R.from_matrix(rw[names['calf_'+s]][:3,:3]));update('foot_'+s,position=goal,rotation=footqs[s])
 update('head',rotation=R.from_matrix(rw[names['head']][:3,:3]))

 vals={}
 for i in g.d['skins'][0]['joints']:
  n=g.nodes[i].get('name')
  if n not in ref:continue
  if i==68:vals[n]=ref[n];continue
  tr=C@lp[i]*100*(wrap if i in tops else 1);q=R.from_matrix(C@lq[i].as_matrix()@C).as_quat();sc=ls[i]*(wrap if i in tops else 1);vals[n]={'translation':tr.tolist(),'rotation':q.tolist(),'scale':sc.tolist()}
 out.append(vals)
# Close the selected cycle with a six-frame blend, preserving quaternion hemisphere.
for j in range(6):
 t=(j+1)/6;idx=len(out)-6+j
 for n in out[idx]:
  for key in ['translation','scale']:out[idx][n][key]=(np.array(out[idx][n][key])*(1-t)+np.array(out[0][n][key])*t).tolist()
  qa=np.array(out[idx][n]['rotation']);qb=np.array(out[0][n]['rotation']);qb*=1 if qa@qb>=0 else -1;q=qa*(1-t)+qb*t;out[idx][n]['rotation']=(q/np.linalg.norm(q)).tolist()
(folder/'walk_tracks.json').write_text(json.dumps({'fps':60,'frames':out,'source':'CMU 07_01','start':start,'end':end,'scale':scale}),encoding='utf-8')
print('cycle',start,end,'seconds',cycle_time,'keys',count,'match',err,'scale',scale)
