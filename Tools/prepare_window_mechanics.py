"""Semantic pane extraction: keep masonry/outer frames fixed; move only actual sashes.
Run in UE editor Python after exporting geometry with CourseworkTools.
"""
import json,pathlib,collections
root=pathlib.Path('D:/HouseCoursework');out=root/'Saved/VentilationAudit'
def components(ts):
 points={};parent=[]
 def key(v):return tuple(round(a,2) for a in v)
 def find(a):
  while parent[a]!=a:parent[a]=parent[parent[a]];a=parent[a]
  return a
 for t in ts:
  ks=[key(p) for p in t['p']]
  for k in ks:
   if k not in points:points[k]=len(parent);parent.append(len(parent))
  for k in ks[1:]:parent[find(points[k])]=find(points[ks[0]])
 groups=collections.defaultdict(list)
 for t in ts:groups[find(points[key(t['p'][0])])].append(t)
 return list(groups.values())
def bounds(ts):
 ps=[p for t in ts for p in t['p']];return [min(p[a] for p in ps) for a in range(3)],[max(p[a] for p in ps) for a in range(3)]
def generate():
 meshes=json.loads((out/'geometry.json').read_text(encoding='utf-8-sig'));recipes=[];layout=[]
 for m in meshes:
  stem=m['path'].split('.')[-1];ts=m['triangles'];groups=components(ts);parts=[];used=set()
  # Original pane subframes have 68 (double hung) or 96 (horizontal slider)
  # triangles. The 208-triangle ring is the stationary outer frame, never a leaf.
  glass={i for i,n in enumerate(m['materials']) if 'Glass' in n}
  pane_frames=[g for g in groups if len(g) in (68,76,96) and bounds(g)[1][1]-bounds(g)[0][1]<8 and 20<bounds(g)[1][0]-bounds(g)[0][0]<100 and 15<bounds(g)[1][2]-bounds(g)[0][2]<150]
  if not pane_frames: continue
  # Use the lower leaf of each vertical pair; horizontal units use center/right.
  if any(len(g)==68 for g in pane_frames):
   frames=[g for g in pane_frames if len(g)==68]
   zmin=min(bounds(g)[0][2] for g in frames); selected_frames=[g for g in frames if bounds(g)[0][2]<zmin+5]
   mode='awning' if 'Floor1_A5_' in stem else 'vertical_slide'
  else:
   frames=sorted(pane_frames,key=lambda g:bounds(g)[0][0]);selected_frames=[frames[len(frames)//2]];mode='horizontal_slide'
  for i,g in enumerate(sorted(selected_frames,key=lambda g:bounds(g)[0])):
   lo,hi=bounds(g); selected=list(g)
   # Include glazing and decorative bars belonging to this pane, using centroid
   # rather than a broad enclosing outer-frame box (which also catches siblings).
   for h in groups:
    if h is g or h in pane_frames:continue
    hl,hh=bounds(h);cx=(hl[0]+hh[0])/2;cz=(hl[2]+hh[2])/2
    if (len(h)==2 or len(h)==16) and lo[0]-4<cx<hi[0]+4 and lo[2]+1<cz<hi[2]-1 and hl[0]>lo[0]-4 and hh[0]<hi[0]+4 and hl[2]>lo[2]-4 and hh[2]<hi[2]+4 and hh[1]-hl[1]<5:
     selected+=h
   ids={t['id'] for t in selected};assert not used&ids;used|=ids
   lo,hi=bounds(selected); center=[(lo[a]+hi[a])/2 for a in range(3)];extent=[(hi[a]-lo[a])/2 for a in range(3)]
   travel=[0,0,0]
   if mode=='vertical_slide': travel[2]=(hi[2]-lo[2])-4
   elif mode=='awning': pass
   else:
    others=[bounds(h) for h in frames if h is not g]; nearest=min(others,key=lambda b:abs((b[0][0]+b[1][0])/2-center[0]));travel[0]=(nearest[0][0]+nearest[1][0])/2-center[0]
   pivot=list(center);axis=[0,0,0];angle=0
   if mode=='awning':pivot[2]=hi[2];axis=[1,0,0];angle=40
   # Target the full visible opening, while airflow uses only the moving pane gap.
   if mode=='vertical_slide':
    pl=list(lo);ph=list(hi);ph[2]+=travel[2]
   else: pl,ph=bounds([t for h in frames for t in h])
   pc=[(pl[a]+ph[a])/2 for a in range(3)];pe=[(ph[a]-pl[a])/2 for a in range(3)];pe[1]=12
   asset='/Game/Coursework/Ventilation/'+stem+'_Sash'+str(i)
   recipes.append(dict(source=m['path'],destination=asset,triangles=sorted(ids)))
   parts.append(dict(asset=asset,pivot=pivot,center=center,extent=extent,travel=travel,axis=axis,angle=angle,proxy_center=pc,proxy_extent=pe,mode=mode,triangles=len(ids)))
  frame='/Game/Coursework/Ventilation/'+stem+'_Frame';recipes.append(dict(source=m['path'],destination=frame,triangles=[t['id'] for t in ts if t['id'] not in used]));layout.append(dict(source=m['path'],frame=frame,parts=parts,total_triangles=len(ts)))
 (out/'parts_recipe.json').write_text(json.dumps(recipes,indent=2),encoding='utf-8');(root/'Tools/Data/ventilation_window_parts.json').write_text(json.dumps(layout,indent=2),encoding='utf-8')
 def vec(v):return 'FVector('+','.join(f'{x:.5f}f' for x in v)+')'
 lines=['// Actual moving pane geometry; fixed outer rings remain in the Frame mesh.','struct FWindowPartSpec {const TCHAR* Source;const TCHAR* Part;FVector Pivot,Center,Extent,Travel,Axis,ProxyCenter,ProxyExtent;float Angle;};','static const FWindowPartSpec WindowParts[]={']
 for m in layout:
  for p in m['parts']:lines.append('{TEXT("'+m['source'].split('.')[-1]+'"),TEXT("'+p['asset'].split('/')[-1]+'"),'+','.join(vec(p[k]) for k in ['pivot','center','extent','travel','axis','proxy_center','proxy_extent'])+f', {p["angle"]:.1f}f'+'},')
 lines.append('};');(root/'Source/HouseCoursework/VentilationWindows.inl').write_text('\n'.join(lines)+'\n',encoding='utf-8');return recipes
if __name__=='__main__':
 recipes=generate()
 import unreal
 result=unreal.CourseworkTools.prepare_window_parts(str(out/'parts_recipe.json'));unreal.log(result);assert result.startswith('SAVED'),result
 unreal.load_module('StaticMeshEditor');sys=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
 for r in recipes:
  mesh=unreal.load_asset(r['destination'])
  for section in range(mesh.get_num_sections(0)):
   mat=mesh.get_material(sys.get_lod_material_slot(mesh,0,section))
   if mat and mat.get_base_material().get_editor_property('blend_mode')==unreal.BlendMode.BLEND_TRANSLUCENT:sys.enable_section_cast_shadow(mesh,False,0,section)
  unreal.EditorAssetLibrary.save_loaded_asset(mesh)
 unreal.log('SEMANTIC_WINDOW_PARTS_COMPLETE')
