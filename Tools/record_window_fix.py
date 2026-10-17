"""Bind the final window tests to the exact local candidate; keep historical evidence separate."""
from pathlib import Path
import hashlib,json,subprocess
repo=Path('D:/HouseCoursework');e=repo/'Docs/WindowMechanicsFix/Final';game=Path('D:/HouseCoursework_TabFinal/Windows')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,data):p.write_bytes((json.dumps(data,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
def check(name,count):
 p=e/name;d=read(p);rows=d['checks'] if isinstance(d,dict) else d;assert len(rows)==count and all(x['passed'] for x in rows),name
 return dict(checks=len(rows),sha256=sha(p))
# Normalize evidence before taking byte fingerprints; Git attributes preserve LF.
for p in (repo/'Docs/WindowMechanicsFix').rglob('*'):
 if p.is_file() and p.suffix in ('.json','.log','.txt'):p.write_bytes(p.read_bytes().replace(b'\r\n',b'\n'))
reports={}
for h in [720,1080]:
 for stem,count in [('ventilation',141),('fixture',251),('daylight',35),('control',30)]:reports[f'{stem}_{h}.json']=check(f'{stem}_{h}.json',count)
 perf=read(e/f'performance_{h}.json');assert len(perf['runs'])==2 and all(x['passed'] and x['average_fps']>=25 for x in perf['runs']);reports[f'performance_{h}.json']=dict(sha256=sha(e/f'performance_{h}.json'),runs=perf['runs'])
for name,count in [('lighting.json',45),('ui.json',9),('map.json',9),('window_dx12.json',89)]:reports[name]=check(name,count)
route=read(e/'ContinuousRouteAcceptance.json');assert route['completed'] and len(route['stops'])==47 and all(x['passed'] for x in route['stops']) and route['average_fps']>=25
assert str(game).replace('\\','/') in (e/'route.log').read_text(encoding='utf-8-sig')
reports['ContinuousRouteAcceptance.json']=dict(sha256=sha(e/'ContinuousRouteAcceptance.json'),stops=47,average_fps=route['average_fps'])
for stem in ['ventilation','fixture','daylight']:
 p=e/(stem+'_summary.json');s=read(p);assert s['passed'] and s['user_save_unchanged'];s['visual_reviewed']=True;write(p,s)
assert read(e/'summary.json')['packaged_fonts_verified'];assert read(e/'rendering_dx12.json')['passed']
assert 'D3D12' in (e/'window_dx12.log').read_text(encoding='utf-8-sig')
# Scope includes all runtime sources and the asset-generation workflow, normalized
# to LF so checkout line endings cannot invalidate a correct source snapshot.
source={}
for directory in ['Source/HouseCoursework','Tools']:
 for p in (repo/directory).rglob('*'):
  if p.is_file() and p.suffix in ('.cpp','.h','.inl','.py','.ps1','.json'):
   source[p.relative_to(repo).as_posix()]=hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest()
assets={p.relative_to(repo).as_posix():sha(p) for p in (repo/'Content/Coursework/Ventilation').rglob('*.uasset')}
exe=game/'HouseCoursework/Binaries/Win64/HouseCoursework.exe';package={p.relative_to(game).as_posix():sha(p) for p in (game/'HouseCoursework/Content/Paks').iterdir() if p.is_file()}
write(e/'rendering_environment.json',dict(renderer='D3D11',compatibility_renderer='D3D12',method='Packaged offscreen rendering; real scene collision and synthetic E/Slate input; no physical desktop input claim',candidate_executable_sha256=sha(exe),normal_user_save_preserved_by_test_scripts=True))
write(e/'validation_scope.json',dict(version='v0.16.4-window-mechanics',baseline_version='v0.16.3-bug-audit',candidate_executable_sha256=sha(exe),candidate_pdb_sha256=sha(game/'HouseCoursework/Binaries/Win64/HouseCoursework.pdb'),package_assets=package,source_files_normalized_lf_sha256=source,ventilation_assets_sha256=assets,fresh_reports=reports,visual_reviewed=['All 16 window open/closed views at 720p; user-reported windows at 1080p and D3D12','Master, child, both study windows, side room, dining and kitchen corner windows','Original materials/rails retained; cloth movement in actual rendered frame pair','20 fixture off/dim views and four isolated off/glow/on triplets','Daylight morning/noon/evening/night'],not_repeated_on_this_binary=['131 furniture parts x 2 cycles','Full resident idle/evacuation suite','Nine-phase lighting persistence','17 menu transition cases at three resolutions'],inherited_baseline_evidence='Docs/BugAudit20261017/Baseline and Final at v0.16.3-bug-audit; these are not claimed as fresh checks',diagnostic_only=['First','Iteration2','Probe','ReferenceRoute'],route_screenshot_overhead_included=True))
print('Bound final evidence to',sha(exe),'with',len(reports),'fresh reports.')
