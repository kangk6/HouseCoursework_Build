"""Record fresh checks separately from unchanged-feature baseline evidence."""
import hashlib,json
from pathlib import Path
repo=Path('D:/HouseCoursework');root=repo/'Docs/HallLightingFix/Final'
def read(name):return json.loads((root/name).read_text(encoding='utf-8-sig'))
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
minimums={'fixture_720.json':251,'fixture_1080.json':251,'hall_d3d12.json':83,'energy_720.json':38,'energy_1080.json':38,'control_720.json':30,'control_1080.json':30,'lighting.json':45,'ui.json':9,'map.json':9}
fresh={}
for name,count in minimums.items():
    data=read(name);checks=data['checks'] if isinstance(data,dict) else data
    assert len(checks)==count and all(row['passed'] for row in checks),(name,'checks failed')
    fresh[name]={'sha256':sha(root/name),'checks':len(checks)}
route=read('ContinuousRouteAcceptance.json')
assert route['completed'] and route['mid_route_teleports']==0 and route['average_fps']>=25 and all(row['passed'] for row in route['stops'])
fresh['ContinuousRouteAcceptance.json']={'sha256':sha(root/'ContinuousRouteAcceptance.json'),'stops':len(route['stops']),'average_fps':route['average_fps']}
hall=read('hall_visual_summary.json');fixtures=read('fixture_summary.json');energy=read('energy_summary.json')
assert hall['passed'] and hall['visual_reviewed'] and fixtures['visual_reviewed'] and energy['visual_reviewed']
for name in ['hall_visual_summary.json','fixture_summary.json','energy_summary.json','summary.json']:fresh[name]={'sha256':sha(root/name)}
baseline=repo/'Docs/VentilationLightFix/Final'
inherited={}
for name in ['ventilation_720.json','ventilation_1080.json','validation_scope.json']:
    inherited[str((baseline/name).relative_to(repo))]={'sha256':sha(baseline/name),'reason':'Ventilation/doors/fire/daylight/persistence behavior unchanged; see baseline scope for earlier inheritance. Not rerun on this binary.'}
source={}
for folder in ['Source','Config']:
    for path in sorted((repo/folder).rglob('*')):
        if path.is_file():source[str(path.relative_to(repo))]=hashlib.sha256(path.read_bytes().replace(b'\r\n',b'\n')).hexdigest()
for path in sorted((repo/'Content/Coursework/Lighting').glob('*.uasset')):source[str(path.relative_to(repo))]=sha(path)
report={'baseline_tag':'v0.16.1-ventilation-light-fix','candidate_executable_sha256':hall['candidate_executable_sha256'],'package_assets':hall['package_assets'],
        'fresh_on_current_binary':fresh,'inherited_not_rerun':inherited,'source_sha256_lf_normalized_for_text':source,
        'review':'All 20 fixture dim/OFF images at 1080p, 720p dim overview, four isolated D3D12 triplets, and energy UI screenshots reviewed. Index-to-actor mapping kept per run.',
        'limits':'Synthetic controller/Slate events with real visibility traces and rendered screenshots, not physical user-input replay. Four defective fixtures have isolated rendered illumination tests; remaining fixtures have positive emitter/glow coverage and normal room ON/OFF visual checks. Lamp models use an artistic no-self-shadow policy while architecture retains shadows. Does not certify an absence of all bugs.'}
(root/'validation_scope.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Recorded fresh hall/whole-house lighting, control, energy and route checks; baseline inheritance explicit.')
