"""Bake sampled cubic animation to linear keys for UE 5.3's glTF importer.

Keeps original mesh, skeleton and materials; the source already has 30 fps keys.
The value (middle element of each tangent/value/tangent tuple) is preserved.
"""
import json,struct,pathlib,sys
root=pathlib.Path(__file__).resolve().parents[1]
source=pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else root/'SourceAssets/ResidentIdle/harper-idle.glb'
data=source.read_bytes()
magic,version,total=struct.unpack_from('<III',data)
assert magic==0x46546c67 and version==2 and total==len(data)
size,kind=struct.unpack_from('<II',data,12)
doc=json.loads(data[20:20+size])
offset=20+size
bin_size,bin_kind=struct.unpack_from('<II',data,offset)
assert bin_kind==0x004e4942
blob=bytearray(data[offset+8:offset+8+bin_size])
report=[]
# MakeHuman exports separately rooted planted feet and torso. Explicitly include
# their existing common armature parent as a joint so UE imports all branches.
parents={child:index for index,node in enumerate(doc['nodes']) for child in node.get('children',[])}
for skin in doc.get('skins',[]):
    joints=skin['joints']
    roots=[j for j in joints if parents.get(j) not in joints]
    if len(roots)>1:
        common=parents[roots[0]]
        assert all(parents[r]==common for r in roots)
        node=doc['nodes'][common]
        assert not any(k in node for k in ('translation','rotation','scale','matrix'))
        accessor=doc['accessors'][skin['inverseBindMatrices']]
        view=doc['bufferViews'][accessor['bufferView']]
        start=view.get('byteOffset',0)+accessor.get('byteOffset',0)
        stride=view.get('byteStride',64)
        matrices=b''.join(blob[start+i*stride:start+i*stride+64] for i in range(accessor['count']))
        matrices+=struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
        while len(blob)%4: blob.append(0)
        new_view=len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer':0,'byteOffset':len(blob),'byteLength':len(matrices)})
        blob.extend(matrices)
        skin['inverseBindMatrices']=len(doc['accessors'])
        doc['accessors'].append({'bufferView':new_view,'componentType':5126,'count':len(joints)+1,'type':'MAT4'})
        joints.append(common)
        skin['skeleton']=common
for animation in doc.get('animations',[]):
    for sampler in animation['samplers']:
        if sampler.get('interpolation')!='CUBICSPLINE': continue
        accessor=doc['accessors'][sampler['output']]
        view=doc['bufferViews'][accessor['bufferView']]
        count=doc['accessors'][sampler['input']]['count']
        assert accessor['componentType']==5126 and accessor['count']==count*3
        width={'VEC3':3,'VEC4':4}[accessor['type']]*4
        stride=view.get('byteStride',width)
        start=view.get('byteOffset',0)+accessor.get('byteOffset',0)
        values=b''.join(blob[start+(i*3+1)*stride:start+(i*3+1)*stride+width] for i in range(count))
        while len(blob)%4: blob.append(0)
        new_view=len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer':0,'byteOffset':len(blob),'byteLength':len(values)})
        blob.extend(values)
        new_accessor=len(doc['accessors'])
        doc['accessors'].append({'bufferView':new_view,'componentType':5126,'count':count,'type':accessor['type']})
        sampler['output']=new_accessor
        sampler['interpolation']='LINEAR'
        first=struct.unpack('<'+str(width//4)+'f',values[:width])
        last=struct.unpack('<'+str(width//4)+'f',values[-width:])
        seam=min(max(abs(a-b) for a,b in zip(first,last)),max(abs(a+b) for a,b in zip(first,last))) if accessor['type']=='VEC4' else max(abs(a-b) for a,b in zip(first,last))
        assert seam<0.0001, 'Non-seamless source animation'
        report.append({'samples':count,'type':accessor['type'],'endpoint_error':seam})
doc['buffers'][0]['byteLength']=len(blob)
encoded=json.dumps(doc,separators=(',',':')).encode()
encoded+=b' '*((-len(encoded))%4)
blob+=b'\0'*((-len(blob))%4)
output=struct.pack('<III',magic,2,12+8+len(encoded)+8+len(blob))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(blob),bin_kind)+blob
source.with_name(source.stem+'-linear.glb').write_bytes(output)
(source.parent/'conversion.json').write_text(json.dumps(report,indent=2))
print('Converted',len(report),'sampled cubic tracks to linear keys')
