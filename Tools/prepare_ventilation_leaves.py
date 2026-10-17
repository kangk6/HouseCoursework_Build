"""Create a separate leaf-only WPO material; preserve all source textures/parameters."""
import unreal
result=unreal.CourseworkTools.prepare_ventilation_leaves()
unreal.log(result)
assert result.startswith('SAVED'),result
unreal.log('VENTILATION_LEAF_MATERIAL_READY')
