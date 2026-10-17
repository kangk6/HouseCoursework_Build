import json,re,pathlib
root=pathlib.Path(__file__).resolve().parents[1]/'Docs/ComfortLighting'
def read(name):return json.loads((root/name).read_text(encoding='utf-8-sig'))
lighting=read('LightingAcceptance.json')
assert len(lighting)==10 and all(c['passed'] for c in lighting)
for room in [0,1]:
 cases=[c for c in lighting if c['room']==room]
 assert [c['action'] for c in cases]==[0,1,2,3,4]
 assert [c['enabled'] for c in cases]==[True,True,False,True,True]
 assert [c['brightness'] for c in cases]==[1,.35,.35,.35,1]
 assert all(c['glows']>0 and c['lights']>0 for c in cases)
log=(root/'lighting.log').read_text(encoding='utf-8-sig')
assert not re.search(r'GLOW_ASSIGNED .* 0\s*$',log,re.M)
settled=re.findall(r'(?:LIGHT_SETTLED \d+ \S+|GLOW_SETTLED \d+) ([\d.]+) EXPECTED ([\d.]+)',log)
assert len(settled)>=80 and all(abs(float(a)-float(b))<.01 for a,b in settled)
route=read('ContinuousRouteAcceptance.json')
assert route['completed'] and route['mid_route_teleports']==0 and all(s['passed'] for s in route['stops'])
assert route['average_fps']>=25
for name in ['CourseworkUIAcceptance.json','CourseworkMapAcceptance.json']:
 c=read(name);assert len(c)==9 and all(r['passed'] for r in c)
idle=read('idle_summary.json');assert idle['feet_position_stable'] and idle['completed_loops']>=2
summary={'lighting_checks':10,'rooms':['living','master_bedroom'],'settled_light_and_material_checks':len(settled),'material_assignment_verified':True,'ui_checks':9,'map_checks':9,'continuous_route_stops':len(route['stops']),'mid_route_teleports':0,'route_fps_1280x720':route['average_fps'],'route_max_frame_ms':route['max_frame_ms'],'idle_loops':idle['completed_loops'],'scope':'Independent Windows package, default night map; same build for lighting and regressions; no full 131-part furniture rerun in this phase'}
(root/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf8')
print(json.dumps(summary,indent=2))
