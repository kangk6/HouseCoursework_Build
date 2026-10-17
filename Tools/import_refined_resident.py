import unreal, json, pathlib

SOURCE = str(pathlib.Path(__file__).resolve().parents[1] / 'SourceAssets/ResidentRefined/standing-relaxed.glb')
DEST = '/Game/Coursework/ResidentRefined'
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters()
params.is_automated = True
if not manager.import_asset(DEST, manager.create_source_data(SOURCE), params):
    raise RuntimeError('Resident import failed')
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
assets = []
for path in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True):
    asset = unreal.load_asset(path)
    entry = {'path': path, 'type': asset.get_class().get_name()}
    if isinstance(asset, unreal.StaticMesh):
        box = asset.get_bounding_box()
        entry.update(min=[box.min.x,box.min.y,box.min.z], max=[box.max.x,box.max.y,box.max.z],
                     materials=[str(m.material_interface.get_path_name()) if m.material_interface else None for m in asset.static_materials])
    assets.append(entry)
pathlib.Path('D:/House_assets/ResidentRefined/import.json').write_text(json.dumps(assets,indent=2),encoding='utf8')
unreal.log('REFINED_RESIDENT_IMPORTED')
