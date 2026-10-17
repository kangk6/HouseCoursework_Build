"""Finalize v0.16.1 provenance after reviewing the current rendered evidence."""
import datetime, hashlib, json, pathlib, shutil
repo=pathlib.Path(__file__).resolve().parent.parent
root=repo/'Docs/VentilationLightFix/Final'
prior=repo/'Docs/Ventilation/Final'
exe=pathlib.Path('D:/HouseCoursework_TabFinal/Windows/HouseCoursework/Binaries/Win64/HouseCoursework.exe')
def read(name):return json.loads((root/name).read_text(encoding='utf-8-sig'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
fresh=[]
for height in (720,1080):
 for prefix,count in [('ventilation',110),('fixture',207),('energy',38)]:
  name=f'{prefix}_{height}.json';data=read(name)
  assert len(data['checks'])==count and all(c['passed'] for c in data['checks']),name
  fresh.append(name)
 name=f'performance_{height}.json';data=read(name)
 assert len(data['runs'])==2 and all(r['passed'] and r['average_fps']>=25 for r in data['runs'])
 fresh.append(name)
for name,count in [('control_720.json',30),('control_1080.json',30),('lighting.json',45),('ui.json',9),('map.json',9)]:
 data=read(name);assert len(data)==count and all(c['passed'] for c in data),name;fresh.append(name)
route=read('ContinuousRouteAcceptance.json')
assert route['completed'] and route['mid_route_teleports']==0 and route['average_fps']>=25 and all(c['passed'] for c in route['stops'])
fresh+=['ContinuousRouteAcceptance.json','desk_d3d12.json','airflow_physics.json']
assert read('airflow_physics.json')['passed']
assert all(c['passed'] for c in read('desk_d3d12.json')['checks'])
for name in fresh:assert (root/name).stat().st_mtime>=exe.stat().st_mtime,f'Stale evidence: {name}'
for name in ['ventilation_summary.json','fixture_summary.json','energy_summary.json','desk_visual_summary.json']:
 data=read(name);assert data['passed'] and data.get('visual_reviewed'),f'Visual review missing: {name}'
sources=list((repo/'Source').rglob('*.cpp'))+list((repo/'Source').rglob('*.h'))+list((repo/'Source').rglob('*.inl'))+list((repo/'Config').glob('*.ini'))+[repo/'HouseCoursework.uproject']
for path in sources:assert path.stat().st_mtime<=exe.stat().st_mtime,f'Unbuilt application source: {path}'
inherited=['persistence_summary.json','daylight_summary.json','daylight_720.json','daylight_1080.json','smoothing_summary.json','sweep_environment.json','evacuation_summary.json','ground.json','upper.json','blocked.json']
for name in inherited:shutil.copy2(prior/name,root/name)
source_hashes={str(p.relative_to(repo)).replace('\\','/'):hashlib.sha256(p.read_text(encoding='utf-8-sig').encode('utf-8')).hexdigest() for p in sources}
scope=dict(baseline_tag='v0.16.0-ventilation',candidate_executable_sha256=sha(exe),
 fresh_on_current_binary={n:sha(root/n) for n in fresh},
 inherited_from_baseline={n:sha(root/n) for n in inherited},
 inheritance_reason='Lighting storage, daylight interpolation, fire and resident evacuation implementation unchanged. Current run freshly exercises energy save restoration, E/Tab synchronization, real window collisions, fire priority and navigation.',
 application_sources_normalized_sha256=source_hashes,
 limits='Fixed east-to-west exterior wind and linear relative-pressure network with collision-tested illustration paths; not CFD. Full regression uses D3D11 offscreen; D3D12 smoke covers both desk lamps at 1080p. Synthetic controller/Slate events, not physical mouse replay.')
(root/'validation_scope.json').write_text(json.dumps(scope,indent=2),encoding='utf-8')
(root/'rendering_environment.json').write_text(json.dumps(dict(full_regression='D3D11 offscreen, 1280x720 and 1920x1080',desk_smoke='D3D12 offscreen, 1920x1080',physical_mouse=False),indent=2),encoding='utf-8')
summary=dict(passed=True,visual_reviewed=True,fresh_current_binary=True,candidate_executable_sha256=sha(exe),
 date_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),living_sashes=4,kitchen_sashes=1,
 rollback_tag='v0.16.0-ventilation',evidence_scope='See validation_scope.json for fresh versus inherited checks')
(root/'bugfix_summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print('Final provenance verified; current binary, 110 ventilation / 207 fixture / 38 energy checks per resolution, controls, route, D3D12 desk smoke and independent flux checks')
