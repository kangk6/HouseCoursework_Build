"""Audit final serialized tracks, including the loop seam, on Harper geometry."""
from gltf_math import *
from arm_clearance import SuitClearance
g=GLB(sys.argv[1]);names={n.get('name'):i for i,n in enumerate(g.nodes)}
C=np.array([[1,0,0],[0,0,1],[0,1,0]]);wrap=g.nodes[69]['scale'][0]
folder=Path(__file__).parent;suit=SuitClearance(g);angles=[];penetration=[]
for frame in json.loads((folder/'walk_tracks.json').read_text())['frames']:
 lp,lq,ls=loc=g.local()
 for n,v in frame.items():
  if n not in names or names[n]==68:continue
  i=names[n];factor=wrap if i in [1,3,55] else 1
  lp[i]=C@np.array(v['translation'])/100/factor;lq[i]=R.from_matrix(C@R.from_quat(v['rotation']).as_matrix()@C);ls[i]=np.array(v['scale'])/factor
 world=g.world(loc);axis=world[names['neck_01']][:3,3]-world[names['pelvis']][:3,3]
 angles.append(np.degrees(np.arctan2(axis[[2,0]],axis[1])))
 cloth=suit.skin(suit.cloth,world)
 penetration.append(max(suit.penetration(suit.skin(suit.hands[s],world),cloth,s,margin=0) for s in ['l','r']))
a=np.array(angles);report={'frames':len(a),'torso_forward_side_degrees':{'mean':a.mean(0).tolist(),'min':a.min(0).tolist(),'max':a.max(0).tolist()},'max_hand_torso_envelope_penetration_cm':max(penetration)*100,'limitations':'Conservative torso cross sections of this suit, not complete garment collision; native rendering is required.'}
(folder/'clearance_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
assert abs(a[:,0].mean())<3 and abs(a[:,1].mean())<1
assert max(penetration)<.002
