import json,pathlib,math,sys
source=pathlib.Path(sys.argv[1])
samples=json.loads(source.read_text(encoding='utf-8-sig'))
assert len(samples)>=27
assert all(s['playing'] and s['looping'] for s in samples)
wraps=sum(b['animation_time']<a['animation_time'] for a,b in zip(samples,samples[1:]))
assert wraps>=2, 'Animation did not complete two loops'
names=[b['name'] for b in samples[0]['bones']]
spans={}
for name in names:
    poses=[next(b for b in s['bones'] if b['name']==name) for s in samples]
    position=[max(p['position'][i] for p in poses)-min(p['position'][i] for p in poses) for i in range(3)]
    rotation=[max(p['rotation'][i] for p in poses)-min(p['rotation'][i] for p in poses) for i in range(3)]
    spans[name]={'position_span_cm':position,'rotation_span_degrees':rotation}
assert all(max(spans[n]['position_span_cm'])<0.1 for n in ['foot_l','foot_r']), 'Foot sliding detected'
assert any(max(v['rotation_span_degrees'])>0.1 for n,v in spans.items() if 'spine' in n or 'head' in n), 'Torso/head remained static'
result={'samples':len(samples),'completed_loops':wraps,'playing_and_looping':True,'feet_position_stable':True,'torso_head_motion':True,'bone_spans':spans}
output=source.with_name('idle_summary.json')
output.write_text(json.dumps(result,indent=2),encoding='utf8')
print(json.dumps(result,indent=2))
