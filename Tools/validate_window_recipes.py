"""Verify that semantic extraction preserves every source triangle exactly once."""
import json,pathlib,collections,hashlib
r=pathlib.Path('D:/HouseCoursework');src=r/'Saved/VentilationAudit/geometry.json';meshes={m['path']:m for m in json.loads(src.read_text(encoding='utf-8-sig'))};recipes=json.loads((r/'Saved/VentilationAudit/parts_recipe.json').read_text(encoding='utf-8'));layout=json.loads((r/'Tools/Data/ventilation_window_parts.json').read_text(encoding='utf-8'))
import sys
sys.path.insert(0,str(r/'Tools'));from prepare_window_mechanics import components
proof=[]
for m in layout:
 groups=[p for p in recipes if p['source']==m['source']];counts=collections.Counter(t for p in groups for t in p['triangles']);ts=meshes[m['source']]['triangles'];assert set(counts)=={t['id'] for t in ts} and all(n==1 for n in counts.values())
 moving={t for p in groups if not p['destination'].endswith('_Frame') for t in p['triangles']}
 rings=[g for g in components(ts) if len(g)==208];assert rings and not any(t['id'] in moving for g in rings for t in g)
 for p in m['parts']:
  assert p['triangles'] in (72,100,120),(m['source'],p['triangles']);assert p['mode']=='awning' or any(abs(v)>50 for v in p['travel'])
 proof.append(dict(source=m['source'],source_triangles=len(ts),moving_parts=len(m['parts']),complete_nonoverlapping_partition=True,fixed_outer_rings=len(rings),modes=[p['mode'] for p in m['parts']]))
out=r/'Docs/WindowMechanicsFix';out.mkdir(exist_ok=True,parents=True);(out/'geometry_validation.json').write_text(json.dumps(dict(passed=True,input_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),meshes=proof),indent=2),encoding='utf-8');print('Validated',len(proof),'source meshes; all triangle partitions complete and all outer rings fixed.')
