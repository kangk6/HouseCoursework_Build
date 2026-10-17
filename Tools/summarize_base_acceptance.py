import json,pathlib,math
root=pathlib.Path(__file__).resolve().parents[1]/'Docs/BaseAcceptance'
def read(name): return json.loads((root/name).read_text(encoding='utf-8-sig'))
furniture=read('FurnitureAcceptance.json'); inventory=read('FurnitureInventory.json')
route=read('ContinuousRouteAcceptance.json')
assert len(furniture)==len(inventory)==131
assert all(r['passed'] and r['cycles_completed']==2 for r in furniture)
expected={(r['actor'],r['component']) for r in inventory}
assert expected=={(r['actor'],r['component']) for r in furniture}
assert route['completed'] and route['mid_route_teleports']==0 and all(s['passed'] for s in route['stops'])
assert route['average_fps']>=25
for name in ['CourseworkUIAcceptance.json','CourseworkMapAcceptance.json']:
    cases=read(name); assert len(cases)==9 and all(c['passed'] for c in cases)
idle=read('idle_summary.json'); assert idle['completed_loops']>=2 and idle['feet_position_stable']
oven=next(f for f in furniture if 'Oven' in f['actor'])
assert oven['attached_decor']>=1 and oven['decor_moved']
audit=read('scene_audit.json'); assert not audit['empty_mesh_components'] and not audit['missing_material_slots']
provenance=read('validation_provenance.json')
summary={'validation_strategy':provenance['strategy'],'furniture_parts':len(furniture),'open_close_cycles_per_part':2,'E_actions_expected':len(furniture)*4,'continuous_route_stops':len(route['stops']),'continuous_route_completed':True,'mid_route_teleports':0,'route_fps_1280x720':route['average_fps'],'route_max_frame_ms':route['max_frame_ms'],'map_checks':9,'ui_checks':9,'idle_completed_loops':idle['completed_loops'],'oven_towel_moves_and_returns':True,'source_missing_mesh_or_material':False,'scope':'This local machine at 1280x720; route includes movement, brief room observation, E-triggered door animations; not an instructor score guarantee'}
(root/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(summary,ensure_ascii=False,indent=2))
