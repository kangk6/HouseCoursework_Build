"""Independently check logged flux budgets and exported trajectory endpoints."""
import json, pathlib, re, sys
root=pathlib.Path(sys.argv[1])
reports=[]
def vector(text):
 return {a:float(b) for a,b in re.findall(r'([XYZ])=([\d.-]+)',text)}
for height in (720,1080):
 batches=[];openings={};routes=[]
 text=(root/f'performance_{height}.log').read_text(encoding='utf-8-sig',errors='replace')
 for line in text.splitlines():
  m=re.search(r'AIR_OPENING room=\d+ center=(V\(.*?\)) inward=(V\(.*?\)) flow=([\d.-]+) obstruction=(.*)',line)
  if m:openings[m[1]]=dict(flow=float(m[3]),inward=vector(m[2]),center=vector(m[1]))
  m=re.search(r'AIRFLOW_ROUTE inlet=(V\(.*?\)) outlet=(V\(.*?\)) pressure=([\d.-]+)->([\d.-]+) relative_flux=([\d.-]+)',line)
  if m:routes.append((m[1],m[2],float(m[3]),float(m[4]),float(m[5])))
  if 'VENTILATION_FLOW ' in line:
   if routes:batches.append((openings,routes))
   openings={};routes=[]
 assert batches,f'{height}: no rendered routes'
 openings,routes=batches[-1];sent={};received={}
 for a,b,high,low,flux in routes:
  assert high>low and flux>0,f'{height}: reverse pressure or negative flux'
  sent[a]=sent.get(a,0)+flux;received[b]=received.get(b,0)+flux
 errors=[]
 for key,opening in openings.items():
  flow=opening['flow']
  shown=sent.get(key,0) if flow>0 else -received.get(key,0)
  errors.append(abs(shown-flow))
 assert max(errors)<.002,f'{height}: source/outlet budget mismatch: {errors}'
 paths=(root/f'rendering_{height}.txt').read_text(encoding='utf-8-sig')
 count=0;endpoints=[]
 def dot(a,b):return sum(a.get(axis,0)*b.get(axis,0) for axis in 'XYZ')
 def subtract(a,b):return {axis:a.get(axis,0)-b.get(axis,0) for axis in 'XYZ'}
 def opening_at(point):
  result=min(openings.values(),key=lambda o:dot(subtract(point,o['center']),subtract(point,o['center'])))
  assert dot(subtract(point,result['center']),subtract(point,result['center']))<20**2,f'{height}: path endpoint not at an actual opening'
  return result
 for line in paths.splitlines():
  if 'cross=1' not in line:continue
  points=[vector(s) for s in re.findall(r'V\((.*?)\)',line)]
  assert len(points)>2
  inlet=opening_at(points[0]);outlet=opening_at(points[-1])
  entry=dot(subtract(points[1],points[0]),inlet['inward'])
  exit=dot(subtract(points[-1],points[-2]),outlet['inward'])
  # Side-facing corner windows are valid outlets too. Check their own
  # aperture normals rather than assuming every outlet faces west (-X).
  assert inlet['flow']>0 and outlet['flow']<0,f'{height}: trajectory endpoints disagree with opening flux'
  assert entry>0,f'{height}: inlet segment points outdoors'
  assert exit<0,f'{height}: outlet segment points indoors'
  endpoints.append(dict(inlet=inlet['center'],outlet=outlet['center'],entry_dot_inward=entry,exit_dot_inward=exit))
  count+=1
 assert count>=2
 reports.append(dict(height=height,route_count=len(routes),trajectory_count=count,max_flux_rounding_error=max(errors),endpoints=endpoints,passed=True))
result=dict(passed=True,method='Independent parse of actual runtime opening fluxes, route budgets and first/last world-space segments against each actual aperture inward normal; not CFD validation',runs=reports)
(root/'airflow_physics.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
