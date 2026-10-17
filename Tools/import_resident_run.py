"""Import the CC0 matching Harper run and bind it to the resident skeleton."""
import unreal, pathlib, json, struct
root=pathlib.Path(__file__).resolve().parents[1]
output=root/'Docs/Evacuation'
output.mkdir(parents=True,exist_ok=True)
def bindposes(path):
    data=path.read_bytes();size=struct.unpack_from('<I',data,12)[0]
    doc=json.loads(data[20:20+size]);blob=data[28+size:]
    skin=doc['skins'][0];accessor=doc['accessors'][skin['inverseBindMatrices']]
    view=doc['bufferViews'][accessor['bufferView']];start=view.get('byteOffset',0)+accessor.get('byteOffset',0)
    return {doc['nodes'][j]['name']:struct.unpack_from('<16f',blob,start+i*64) for i,j in enumerate(skin['joints'])}
idle=bindposes(root/'SourceAssets/ResidentIdle/harper-idle-linear.glb')
run=bindposes(root/'SourceAssets/ResidentRun/harper-run-linear.glb')
assert set(idle)==set(run), 'Different joints'
error=max(abs(a-b) for name in idle for a,b in zip(idle[name],run[name]))
assert error<.0001, 'Different reference poses: '+str(error)
dest='/Game/Coursework/ResidentRun'
manager=unreal.InterchangeManager.get_interchange_manager_scripted()
params=unreal.ImportAssetParameters();params.is_automated=True
if not unreal.EditorAssetLibrary.does_asset_exist(dest+'/harper-run-linear_Anim'):
    assert manager.import_asset(dest,manager.create_source_data(str(root/'SourceAssets/ResidentRun/harper-run-linear.glb')),params)
entries=[]
skeleton='/Game/Coursework/ResidentAnimated/harper-idle-linear_Skeleton.harper-idle-linear_Skeleton'
for path in unreal.EditorAssetLibrary.list_assets(dest,recursive=True):
    asset=unreal.load_asset(path)
    if isinstance(asset,unreal.AnimSequence):
        result=unreal.CourseworkTools.bind_resident_animation(path,skeleton)
        assert result.startswith('BOUND'),result
        assert 1.2<asset.sequence_length<1.5
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        entries.append({'path':path,'length':asset.sequence_length,'skeleton':asset.get_editor_property('skeleton').get_path_name()})
assert len(entries)==1
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
(output/'run_import.json').write_text(json.dumps({'bind_pose_max_error':error,'joints':len(idle),'animations':entries},indent=2),encoding='utf8')
unreal.log('EVAC_RUN_IMPORTED '+str(entries))
