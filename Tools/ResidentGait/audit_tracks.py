"""Audit final serialized tracks, including the loop seam, on Harper geometry."""
from gltf_math import *
from arm_clearance import SuitClearance
g=GLB(sys.argv[1]);names={n.get('name'):i for i,n in enumerate(g.nodes)}
C=np.array([[1,0,0],[0,0,1],[0,1,0]]);wrap=g.nodes[69]['scale'][0]
folder=Path(__file__).parent;suit=SuitClearance(g);angles=[];penetration=[]
tracks=json.loads((folder/'walk_tracks.json').read_text())['frames']
for frame in tracks:
 lp,lq,ls=loc=g.local()
 for n,v in frame.items():
  if n not in names or names[n]==68:continue
  i=names[n];factor=wrap if i in [1,3,55] else 1
  lp[i]=C@np.array(v['translation'])/100/factor;lq[i]=R.from_matrix(C@R.from_quat(v['rotation']).as_matrix()@C);ls[i]=np.array(v['scale'])/factor
 world=g.world(loc);axis=world[names['neck_01']][:3,3]-world[names['pelvis']][:3,3]
 angles.append(np.degrees(np.arctan2(axis[[2,0]],axis[1])))
 cloth=suit.skin(suit.cloth,world)
 penetration.append(max(suit.penetration(suit.skin(suit.hands[s],world),cloth,s,margin=0) for s in ['l','r']))
a=np.array(angles);continuity={}
for bone in ['thigh_l','calf_l','thigh_r','calf_r','upperarm_l','lowerarm_l','hand_l','upperarm_r','lowerarm_r','hand_r']:
 q=R.from_quat([f[bone]['rotation'] for f in tracks[:-1]])
 velocity=(q.inv()*R.from_quat(np.roll(q.as_quat(),-1,axis=0))).as_rotvec()*60
 step=np.linalg.norm(velocity,axis=1)*180/np.pi/60
 acceleration=np.linalg.norm(np.roll(velocity,-1,axis=0)-velocity,axis=1)*60*180/np.pi
 continuity[bone]={'max_step_degrees':float(step.max()),'peak_acceleration_degrees_s2':float(acceleration.max()),'seam_acceleration_degrees_s2':float(acceleration[-1]),'endpoint_difference_degrees':float((q[0].inv()*R.from_quat(tracks[-1][bone]['rotation'])).magnitude()*180/np.pi)}
report={'frames':len(a),'torso_forward_side_degrees':{'mean':a.mean(0).tolist(),'min':a.min(0).tolist(),'max':a.max(0).tolist()},'max_hand_torso_envelope_penetration_cm':max(penetration)*100,'continuity':continuity,'limitations':'Conservative torso cross sections of this suit, not complete garment collision; native rendering is required. Local bone derivatives at 60 Hz are animation checks, not runtime FPS.'}
(folder/'clearance_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
assert abs(a[:,0].mean())<3 and abs(a[:,1].mean())<1
assert max(penetration)<.002
assert all(v['max_step_degrees']<8 and v['peak_acceleration_degrees_s2']<15000 and v['endpoint_difference_degrees']<1e-5 for v in continuity.values())
