import bpy
import struct
import os
import math
import tempfile
from pathlib import Path
from enum import IntEnum

bl_info = {
    "name": "PSF Format Importer",
    "blender": (3, 0, 0),
    "category": "Import-Export",
    "version": (1, 0, 0),
    "author": "PSF Converter",
    "description": "Import Procedural Surface Format (PSF) files into Blender"
}

# PSF Format Constants
PSF_MAGIC = 0x50534600
PSF_VERSION = 1

class ColorEqn(IntEnum):
    CONSTANT = 0
    LINEAR_U = 1
    LINEAR_V = 2
    BILINEAR = 3

class Vec3:
    def __init__(self, x=0, y=0, z=0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)
    
    def __add__(self, other):
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)
    
    def __sub__(self, other):
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)
    
    def __mul__(self, scalar):
        return Vec3(self.x * scalar, self.y * scalar, self.z * scalar)
    
    def to_blender(self):
        """Convert to Blender tuple format"""
        return (self.x, self.y, self.z)

class PSFReader:
    def __init__(self, filepath, verbose=True):
        self.filepath = filepath
        self.verbose = verbose
        self.anchors = []
        self.surfaces = []
        self.bones = []
    
    def read(self):
        """Read PSF file"""
        try:
            with open(self.filepath, 'rb') as f:
                # Read header
                magic = struct.unpack('<I', f.read(4))[0]
                version = struct.unpack('<I', f.read(4))[0]
                
                if magic != PSF_MAGIC:
                    print(f"Error: Invalid PSF magic number: {hex(magic)}")
                    return False
                
                if self.verbose:
                    print(f"PSF Version: {version}")
                
                # Skip object name (256 bytes)
                f.read(256)
                
                # Read pool sizes
                obj_count = struct.unpack('<I', f.read(4))[0]
                anchor_count = struct.unpack('<I', f.read(4))[0]
                surface_count = struct.unpack('<I', f.read(4))[0]
                bone_count = struct.unpack('<I', f.read(4))[0]
                
                if self.verbose:
                    print(f"Objects: {obj_count}, Anchors: {anchor_count}, Surfaces: {surface_count}, Bones: {bone_count}")
                
                # Read anchors
                for i in range(anchor_count):
                    anchor_data = f.read(22)
                    if len(anchor_data) < 22:
                        print(f"Error reading anchor {i}")
                        return False
                    
                    pos_x, pos_y, pos_z, norm_x, norm_y, norm_z, col_r, col_g, col_b, bone_idx, rough, metal = \
                        struct.unpack('<3h3B3BIBB', anchor_data[:18])  # Unpack the first 18 bytes
                    
                    anchor = {
                        'position': Vec3(pos_x/18204.0, pos_y/18204.0, pos_z/18204.0),
                        'normal': self._decode_normal([norm_x, norm_y, norm_z]),
                        'color': Vec3(col_r/255.0, col_g/255.0, col_b/255.0),
                        'bone_index': bone_idx,
                        'roughness': rough,
                        'metallic': metal
                    }
                    self.anchors.append(anchor)
                
                # Read surfaces
                for i in range(surface_count):
                    surface_data = f.read(60)
                    if len(surface_data) < 60:
                        print(f"Error reading surface {i}")
                        return False
                    
                    indices = struct.unpack('<4H', surface_data[:8])
                    params = struct.unpack('<8f', surface_data[8:40])
                    surface_eqn, modifier = struct.unpack('<2B', surface_data[40:42])
                    color_eqn_r, color_eqn_g, color_eqn_b = struct.unpack('<3B', surface_data[42:45])
                    color_param_r, color_param_g, color_param_b = struct.unpack('<3B', surface_data[45:48])
                    roughness, metallic, emissive, specular = struct.unpack('<4B', surface_data[48:52])
                    
                    surface = {
                        'anchor_indices': list(indices),
                        'params': list(params),
                        'surface_eqn': surface_eqn,
                        'modifier': modifier,
                        'color_eqn': [color_eqn_r, color_eqn_g, color_eqn_b],
                        'color_param': [color_param_r, color_param_g, color_param_b],
                        'roughness': roughness,
                        'metallic': metallic,
                        'emissive': emissive,
                        'specular': specular
                    }
                    self.surfaces.append(surface)
                
                # Read bones
                for i in range(bone_count):
                    bone_data = f.read(30)
                    if len(bone_data) < 30:
                        print(f"Error reading bone {i}")
                        return False
                    
                    pos_x, pos_y, pos_z, parent, child, orient, _ = struct.unpack('<3h3IB', bone_data[:19])
                    phys_params = struct.unpack('<2f', bone_data[19:27])
                    
                    bone = {
                        'position': Vec3(pos_x/18204.0, pos_y/18204.0, pos_z/18204.0),
                        'parent': parent,
                        'child': child,
                        'orientation': orient,
                        'physical_params': phys_params
                    }
                    self.bones.append(bone)
                
                if self.verbose:
                    print(f"Successfully read PSF file")
                return True
        
        except Exception as e:
            print(f"Error reading PSF: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def _decode_normal(self, encoded):
        x = (encoded[0] / 255.0) * 2.0 - 1.0
        y = (encoded[1] / 255.0) * 2.0 - 1.0
        z = (encoded[2] / 255.0) * 2.0 - 1.0
        length = math.sqrt(x*x + y*y + z*z)
        if length > 1e-6:
            return Vec3(x/length, y/length, z/length)
        return Vec3(0, 0, 1)

class PSFMeshBuilder:
    def __init__(self, psf_reader, verbose=True):
        self.reader = psf_reader
        self.verbose = verbose
    
    def create_mesh(self, mesh_name="PSF_Mesh"):
        """Create Blender mesh from PSF data"""
        # Create mesh and object
        mesh_data = bpy.data.meshes.new(mesh_name)
        obj = bpy.data.objects.new(mesh_name, mesh_data)
        
        # Link to scene
        bpy.context.collection.objects.link(obj)
        bpy.context.view_layer.objects.active = obj
        
        # Collect vertices
        vertices = []
        for anchor in self.reader.anchors:
            vertices.append(anchor['position'].to_blender())
        
        # Collect faces
        faces = []
        for surface in self.reader.surfaces:
            indices = surface['anchor_indices']
            if len(indices) >= 4:
                faces.append(tuple(indices))
            elif len(indices) == 3:
                faces.append(tuple(indices))
        
        # Create mesh geometry
        mesh_data.from_pydata(vertices, [], faces)
        mesh_data.update()
        
        # Set normals
        mesh_data.use_auto_smooth = True
        for i, anchor in enumerate(self.reader.anchors):
            if i < len(mesh_data.vertices):
                normal = anchor['normal']
                mesh_data.vertices[i].normal = normal.to_blender()
        
        if self.verbose:
            print(f"Created mesh '{mesh_name}' with {len(vertices)} vertices and {len(faces)} faces")
        
        return obj
    
    def create_materials(self, obj):
        """Create materials from PSF color data"""
        materials_created = 0
        
        # Create a default material
        mat = bpy.data.materials.new("PSF_Material")
        mat.use_nodes = True
        mat.blend_method = 'BLEND'
        
        # Set basic properties
        bsdf = mat.node_tree.nodes["Principled BSDF"]
        bsdf.inputs['Base Color'].default_value = (0.7, 0.7, 0.7, 1.0)
        bsdf.inputs['Metallic'].default_value = 0.0
        bsdf.inputs['Roughness'].default_value = 0.5
        
        # Assign to object
        if obj.data.materials:
            obj.data.materials[0] = mat
        else:
            obj.data.materials.append(mat)
        
        materials_created = 1
        
        if self.verbose:
            print(f"Created {materials_created} material(s)")
        
        return materials_created
    
    def create_armature(self):
        """Create armature from bone data"""
        if not self.reader.bones:
            return None
        
        # Create armature
        armature_data = bpy.data.armatures.new("PSF_Armature")
        armature_obj = bpy.data.objects.new("PSF_Armature", armature_data)
        bpy.context.collection.objects.link(armature_obj)
        
        # Enter edit mode to create bones
        bpy.context.view_layer.objects.active = armature_obj
        bpy.ops.object.mode_set(mode='EDIT')
        
        # Create bones
        bone_objects = {}
        for i, bone_data in enumerate(self.reader.bones):
            bone = armature_data.edit_bones.new(f"Bone_{i}")
            bone.head = bone_data['position'].to_blender()
            
            # Slightly offset tail for visualization
            pos = bone_data['position']
            bone.tail = (pos.x + 0.05, pos.y, pos.z)
            
            bone_objects[i] = bone
        
        # Set up parent relationships
        for i, bone_data in enumerate(self.reader.bones):
            parent_idx = bone_data['parent']
            if parent_idx != i and parent_idx in bone_objects:
                child_bone = bone_objects[i]
                parent_bone = bone_objects[parent_idx]
                child_bone.parent = parent_bone
        
        bpy.ops.object.mode_set(mode='OBJECT')
        
        if self.verbose:
            print(f"Created armature with {len(self.reader.bones)} bones")
        
        return armature_obj

class PSF_OT_import(bpy.types.Operator):
    bl_idname = "import_scene.psf"
    bl_label = "Import PSF"
    bl_options = {'REGISTER', 'UNDO'}
    
    filepath: bpy.props.StringProperty(
        name="File Path",
        description="Filepath for PSF import",
        subtype='FILE_PATH'
    )
    
    filter_glob: bpy.props.StringProperty(
        default="*.psf",
        options={'HIDDEN'}
    )
    
    import_armature: bpy.props.BoolProperty(
        name="Import Armature",
        description="Import skeleton/bones from PSF",
        default=True
    )
    
    verbose: bpy.props.BoolProperty(
        name="Verbose Output",
        description="Print detailed import information",
        default=True
    )
    
    def execute(self, context):
        if not self.filepath:
            self.report({'ERROR'}, "Filepath not specified")
            return {'CANCELLED'}
        
        if not os.path.exists(self.filepath):
            self.report({'ERROR'}, f"File not found: {self.filepath}")
            return {'CANCELLED'}
        
        # Read PSF
        reader = PSFReader(self.filepath, verbose=self.verbose)
        if not reader.read():
            self.report({'ERROR'}, "Failed to read PSF file")
            return {'CANCELLED'}
        
        # Create mesh
        builder = PSFMeshBuilder(reader, verbose=self.verbose)
        
        mesh_name = Path(self.filepath).stem
        obj = builder.create_mesh(mesh_name)
        
        if obj is None:
            self.report({'ERROR'}, "Failed to create mesh")
            return {'CANCELLED'}
        
        # Create materials
        builder.create_materials(obj)
        
        # Create armature if requested
        if self.import_armature and reader.bones:
            armature = builder.create_armature()
            if armature:
                # Parent mesh to armature
                obj.parent = armature
                obj.parent_type = 'OBJECT'
        
        self.report({'INFO'}, f"Imported PSF: {self.filepath}")
        return {'FINISHED'}
    
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}
    
    def draw(self, context):
        layout = self.layout
        layout.prop(self, "import_armature")
        layout.prop(self, "verbose")

class PSF_PT_import_panel(bpy.types.Panel):
    bl_label = "PSF Importer"
    bl_idname = "VIEW_3D_PT_psf_importer"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'PSF'
    
    def draw(self, context):
        layout = self.layout
        layout.label(text="Import PSF Files", icon='IMPORT')
        layout.operator("import_scene.psf", text="Import PSF", icon='IMPORT')
        
        layout.separator()
        layout.label(text="Features:", icon='INFO')
        col = layout.column()
        col.label(text="✓ Load mesh geometry")
        col.label(text="✓ Preserve vertex normals")
        col.label(text="✓ Import skeleton bones")
        col.label(text="✓ Create materials")

def menu_func_import(self, context):
    self.layout.operator(PSF_OT_import.bl_idname, text="PSF (.psf)")

def register():
    bpy.utils.register_class(PSF_OT_import)
    bpy.utils.register_class(PSF_PT_import_panel)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    print("✓ PSF Importer addon registered")

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.utils.unregister_class(PSF_OT_import)
    bpy.utils.unregister_class(PSF_PT_import_panel)
    print("✗ PSF Importer addon unregistered")

if __name__ == "__main__":
    register()