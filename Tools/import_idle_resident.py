import unreal,json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
source=str(root/'SourceAssets/ResidentIdle/harper-idle-linear.glb')
dest='/Game/Coursework/ResidentAnimated'
manager=unreal.InterchangeManager.get_interchange_manager_scripted()
params=unreal.ImportAssetParameters()
params.is_automated=True
if not manager.import_asset(dest,manager.create_source_data(source),params):
    raise RuntimeError('Idle resident import failed')
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
entries=[]
for path in unreal.EditorAssetLibrary.list_assets(dest,recursive=True):
    asset=unreal.load_asset(path)
    e={'path':path,'type':asset.get_class().get_name()}
    if isinstance(asset,unreal.SkeletalMesh):
        e['skeleton']=str(asset.get_editor_property('skeleton').get_path_name())
        e['bounds']=str(asset.get_bounds())
    if isinstance(asset,unreal.AnimSequence):
        e['length']=asset.sequence_length
        e['skeleton']=str(asset.get_editor_property('skeleton').get_path_name())
    entries.append(e)
output=root/'Docs/ResidentIdle'
output.mkdir(parents=True,exist_ok=True)
(output/'import.json').write_text(json.dumps(entries,indent=2),encoding='utf8')
unreal.log('IDLE_RESIDENT_IMPORTED')
