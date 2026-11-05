import sys,json,struct,copy
from pathlib import Path
import numpy as np
from scipy.spatial.transform import Rotation as R
class GLB:
 def __init__(self,path):
  raw=Path(path).read_bytes();n=struct.unpack_from('<I',raw,12)[0];self.d=json.loads(raw[20:20+n]);self.blob=raw[28+n:];self.nodes=self.d['nodes'];self.parent={c:i for i,n in enumerate(self.nodes) for c in n.get('children',[])}
 def acc(self,i):
  a=self.d['accessors'][i];v=self.d['bufferViews'][a['bufferView']];dims={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}[a['type']];dt={5126:'<f4',5123:'<u2',5125:'<u4',5121:'u1'}[a['componentType']];return np.ndarray((a['count'],dims),dtype=dt,buffer=self.blob,offset=v.get('byteOffset',0)+a.get('byteOffset',0),strides=(v.get('byteStride',np.dtype(dt).itemsize*dims),np.dtype(dt).itemsize)).copy()
 def local(self,t=None):
  pos=[np.array(n.get('translation',[0,0,0]),float) for n in self.nodes];rot=[R.from_quat(n.get('rotation',[0,0,0,1])) for n in self.nodes];scale=[np.array(n.get('scale',[1,1,1]),float) for n in self.nodes]
  if t is not None:
   a=self.d['animations'][0]
   for c in a['channels']:
    s=a['samplers'][c['sampler']];ts=self.acc(s['input'])[:,0];vs=self.acc(s['output']);idx=min(np.searchsorted(ts,t),len(ts)-1);i=c['target']['node'];key=c['target']['path']
    if key=='translation':pos[i]=vs[idx]
    elif key=='rotation':rot[i]=R.from_quat(vs[idx])
    elif key=='scale':scale[i]=vs[idx]
  return pos,rot,scale
 def world(self,local):
  pos,rot,scale=local;mat={}
  def calc(i):
   if i in mat:return mat[i]
   m=np.eye(4);m[:3,:3]=rot[i].as_matrix()@np.diag(scale[i]);m[:3,3]=pos[i];mat[i]=calc(self.parent[i])@m if i in self.parent else m;return mat[i]
  return [calc(i) for i in range(len(self.nodes))]
