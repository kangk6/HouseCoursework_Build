"""Offline clearance against the skinned torso of this specific suit."""
from gltf_math import *
class SuitClearance:
 def __init__(self,g):
  self.g=g;self.joints=g.d['skins'][0]['joints'];self.names=[g.nodes[i].get('name','') for i in self.joints];self.ib=g.acc(g.d['skins'][0]['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
  def arrays(node):
   a=g.d['meshes'][g.nodes[node]['mesh']]['primitives'][0]['attributes'];return g.acc(a['POSITION']),g.acc(a['JOINTS_0']).astype(int),g.acc(a['WEIGHTS_0'])
  cp,cj,cw=arrays(62);allowed=np.array([n=='Root' or n=='pelvis' or n.startswith(('spine','thigh','calf')) for n in self.names]);mask=(allowed[cj]*cw).sum(1)>.8;self.cloth=(cp[mask],cj[mask],cw[mask])
  bp,bj,bw=arrays(60);self.hands={}
  for side in ['l','r']:
   hand=np.array([n.endswith('_'+side) and n.startswith(('hand','thumb','index','middle','ring','pinky')) for n in self.names]);m=(hand[bj]*bw).sum(1)>.35;self.hands[side]=(bp[m],bj[m],bw[m])
 def skin(self,data,world):
  p,j,w=data;m=np.stack([world[i] for i in self.joints])@self.ib;h=np.c_[p,np.ones(len(p))];return (np.einsum('nkij,nj->nki',m[j],h)*w[:,:,None]).sum(1)[:,:3]
 @staticmethod
 def penetration(hand,cloth,side,margin=.006):
  sign=1 if side=='l' else -1;worst=0
  for p in hand:
   close=cloth[(abs(cloth[:,1]-p[1])<.025)&(abs(cloth[:,2]-p[2])<.03)]
   if len(close)<3:continue
   bound=np.max(close[:,0]) if sign>0 else np.min(close[:,0]);worst=max(worst,sign*(bound-p[0])+margin)
  return max(0,worst)
