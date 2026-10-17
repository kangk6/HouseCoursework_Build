"""Distinguish checks on the published baseline from checks on the menu patch."""
import hashlib,json
from pathlib import Path
repo=Path('D:/HouseCoursework');root=repo/'Docs/BugAudit20261017';base=root/'Baseline';final=root/'Final';game=Path('D:/HouseCoursework_TabFinal/Windows')
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def checks(path,count):
    data=read(path);rows=data['checks'] if isinstance(data,dict) else data
    assert len(rows)==count and all(r['passed'] for r in rows),path
    return {'sha256':sha(path),'checks':len(rows)}
runs=read(base/'audit_runs.json');assert len(runs)==5 and all(r['passed'] for r in runs)
furniture=read(base/'FurnitureAcceptance.json')
assert len(furniture)==131 and len({(r['actor'],r['component']) for r in furniture})==131
assert all(r['passed'] and r['cycles_completed']==2 for r in furniture)
idle=read(base/'idle_summary.json');assert idle['playing_and_looping'] and idle['feet_position_stable'] and idle['torso_head_motion'] and idle['completed_loops']>=2
for mode,count in [('ground',22),('upper',22),('blocked',23)]:
    checks(base/(mode+'.json'),count);assert read(base/(mode+'.json'))['average_fps']>=25
persistence=read(base/'persistence_summary.json');assert persistence['passed'] and persistence['phases']==9 and persistence['user_save_unchanged']
for height in [720,1080]:
    checks(base/f'ventilation_{height}.json',110);checks(base/f'daylight_{height}.json',35)
    assert all(r['passed'] and r['average_fps']>=25 for r in read(base/f'performance_{height}.json')['runs'])
for name in ['evacuation_summary.json','ventilation_summary.json','daylight_summary.json']:assert read(base/name)['visual_reviewed']
exe=game/'HouseCoursework/Binaries/Win64/HouseCoursework.exe';modified=exe.stat().st_mtime
fresh={}
for height in [720,1080,1200]:
    path=final/f'transitions_{height}.json';assert path.stat().st_mtime>=modified;fresh[path.name]=checks(path,17)
for name in ['CourseworkUIAcceptance.json','CourseworkMapAcceptance.json']:
    path=final/name;assert path.stat().st_mtime>=modified;fresh[name]=checks(path,9)
path=final/'ContinuousRouteAcceptance.json';assert path.stat().st_mtime>=modified
route=read(path);assert route['completed'] and route['mid_route_teleports']==0 and len(route['stops'])==47 and all(r['passed'] for r in route['stops']) and route['average_fps']>=25
fresh[path.name]={'sha256':sha(path),'stops':47,'average_fps':route['average_fps']}
captured=read(final/'ContinuousRouteWithCapture.json')
assert captured['screenshots'] and not route['screenshots'] and captured['completed'] and len(captured['stops'])==47
fresh['ContinuousRouteWithCapture.json']={'sha256':sha(final/'ContinuousRouteWithCapture.json'),'stops':47,'average_fps':captured['average_fps']}
transitions=read(final/'transitions_summary.json');assert transitions['visual_reviewed'] and transitions['user_save_unchanged'] and transitions['candidate_executable_sha256'].lower()==sha(exe)
source={}
for folder in ['Source','Config']:
    for p in sorted((repo/folder).rglob('*')):
        if p.is_file():source[str(p.relative_to(repo))]=hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest()
history=repo/'Docs/HallLightingFix/Final'
report={'baseline_tag':'v0.16.2-hall-lighting-fix','baseline_binary':read(base/'binary_provenance.json'),
        'candidate_executable_sha256':sha(exe),'fresh_on_current_binary':fresh,
        'fresh_on_baseline_binary':{str(p.relative_to(repo)):{'sha256':sha(p)} for p in sorted(base.glob('*.json'))},
        'inherited_unchanged_lighting_controls_from_v0_16_2':{str((history/name).relative_to(repo)):{'sha256':sha(history/name)} for name in ['summary.json','lighting.json','control_720.json','control_1080.json','fixture_720.json','fixture_1080.json','energy_720.json','energy_1080.json','hall_visual_summary.json']},
        'package_assets':[{'path':str(p.relative_to(game)),'sha256':sha(p)} for p in sorted((game/'HouseCoursework/Content/Paks').glob('*')) if p.is_file()],
        'source_sha256_lf_normalized_for_text':source,
        'behavior_changes':['Active drill Tab opens safety; normal Tab opens lighting.','Opening a panel consumes pending move input and clears current player velocity.'],
        'performance_comparison':{'with_screenshot_capture':{k:captured[k] for k in ['average_fps','max_frame_ms','frames_over_100ms']},'without_screenshot_capture':{k:route[k] for k in ['average_fps','max_frame_ms','frames_over_100ms']},'note':'Automatic screenshot capture is included in the first route and excluded from the second; route/pause/door behavior otherwise uses the same test. Do not label captured long frames as resource-loading defects without profiling.'},
        'remaining_observations':['One 172.307 ms frame at upper_hall_return in the no-capture route. Cause not profiled; record as an unresolved intermittent performance observation, not a proven asset-loading defect.'],
        'limits':'Menu tests call the actual HUD handlers with live movement, pause and UI states, not physical OS keyboard replay. Prior input-only behavior is reproduced as a coasting negative control. Baseline checks and patch checks use different explicitly fingerprinted binaries. Fast repeated E during unfinished animations, arbitrary player positions, multi-hour sessions and hardware variation are not exhaustive. Passing the scoped checks does not certify an absence of all bugs.'}
(final/'validation_scope.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Verified baseline and current-patch evidence separately; 131 furniture parts, baseline feature suites and fresh menu/route/UI/map checks.')
