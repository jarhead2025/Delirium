import bpy
import bmesh
import struct
import numpy as np
from mathutils import Vector, Matrix
import math
from collections import defaultdict
from enum import IntEnum

bl_info = {
    "name": "PSF Format Exporter Pro",
    "blender": (3, 0, 0),
    "category": "Import-Export",
    "version": (2, 0, 0),
    "author": "PSF Converter",
    "description": "Export Blender objects to PSF with intelligent surface merging and skeleton extraction"
}

# PSF Format Constants
PSF_MAGIC = 0x50534600
PSF_VERSION = 1
EQUATION_PLANE = 0
EQUATION_SPHERE = 1
EQUATION_CYLINDER = 2
MOD_COLOR = 1

# Color Equation Types
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
    
    def __truediv__(self, scalar):
        return Vec3(self.x / scalar, self.y / scalar, self.z / scalar)
    
    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)
    
    def normalize(self):
        l = self.length()
        if l > 1e-6:
            return Vec3(self.x/l, self.y/l, self.z/l)
        return Vec3(0, 0, 1)
    
    def dot(self, other):
        return self.x*other.x + self.y*other.y + self.z*other.z
    
    def cross(self, other):
        return Vec3(
            self.y*other.z - self.z*other.y,
            self.z*other.x - self.x*other.z,
            self.x*other.y - self.y*other.x
        )
    
    def clamp(self, min_val, max_val):
        return Vec3(
            max(min_val, min(max_val, self.x)),
            max(min_val, min(max_val, self.y)),
            max(min_val, min(max_val, self.z))
        )
    
    def distance_to(self, other):
        return (self - other).length()
    
    def __repr__(self):
        return f"Vec3({self.x:.3f}, {self.y:.3f}, {self.z:.3f})"

class AnchorPointBuffer:
    def __init__(self):
        self.pos_xyz = [0, 0, 0]
        self.normal = [128, 128, 255]
        self.color = [180, 140, 100]
        self.bone_index = 0
        self.roughness = 128
        self.metallic = 0
    
    def pack(self):
        # Manually pack to avoid format string confusion
        data = struct.pack('<3h', *self.pos_xyz)  # 6 bytes
        data += struct.pack('<3B', *self.normal)  # 3 bytes
        data += struct.pack('<3B', *self.color)   # 3 bytes
        data += struct.pack('<I', self.bone_index)  # 4 bytes
        data += struct.pack('<B', self.roughness)   # 1 byte
        data += struct.pack('<B', self.metallic)    # 1 byte
        data += b'\x00\x00\x00\x00'  # 4 bytes padding
        return data  # Total: 22 bytes
    
    @staticmethod
    def size():
        return 22

class ProceduralSurfaceBuffer:
    def __init__(self):
        self.anchor_point_indices = [0, 0, 0, 0]
        self.params = [0.0] * 8
        self.surface_eqn = EQUATION_PLANE
        self.modifier = MOD_COLOR
        self.color_eqn_r = ColorEqn.CONSTANT
        self.color_eqn_g = ColorEqn.CONSTANT
        self.color_eqn_b = ColorEqn.CONSTANT
        self.color_param_r = 128
        self.color_param_g = 128
        self.color_param_b = 128
        self.roughness = 128
        self.metallic = 0
        self.emissive_strength = 0
        self.specular_tint = 0
    
    def pack(self):
        # Manually pack to ensure correct 60 bytes
        # 4H = 8 bytes (anchor indices)
        # 8f = 32 bytes (params)
        # 12B = 12 bytes (various uint8s)
        # Total so far = 52 bytes, need 8 bytes padding
        data = struct.pack('<4H', *self.anchor_point_indices)  # 8 bytes
        data += struct.pack('<8f', *self.params)  # 32 bytes
        data += struct.pack('<12B', 
            self.surface_eqn, self.modifier,
            self.color_eqn_r, self.color_eqn_g, self.color_eqn_b,
            self.color_param_r, self.color_param_g, self.color_param_b,
            self.roughness, self.metallic, self.emissive_strength, self.specular_tint)  # 12 bytes
        data += b'\x00\x00\x00\x00\x00\x00\x00\x00'  # 8 bytes padding
        return data  # Total: 8+32+12+8 = 60 bytes
    
    @staticmethod
    def size():
        return 60

class BoneBuffer:
    def __init__(self):
        self.pos_xyz = [0, 0, 0]
        self.parent_bone_index = 0
        self.child_bone_index = 0
        self.orientation = 0
        self.physical_params = [1.0, 0.1]
    
    def pack(self):
        # Manually pack to ensure correct 30 bytes
        # 3h = 6 bytes (position)
        # 3I = 12 bytes (parent, child, orientation)
        # 1B = 1 byte (padding field)
        # 2f = 8 bytes (physical params)
        # Total so far = 27 bytes, need 3 bytes padding
        data = struct.pack('<3h', *self.pos_xyz)  # 6 bytes
        data += struct.pack('<3I', self.parent_bone_index, self.child_bone_index, self.orientation)  # 12 bytes
        data += struct.pack('<B', 0)  # 1 byte padding field
        data += struct.pack('<2f', *self.physical_params)  # 8 bytes
        data += b'\x00\x00\x00'  # 3 bytes padding
        return data  # Total: 6+12+1+8+3 = 30 bytes
    
    @staticmethod
    def size():
        return 30

class PositionEncoder:
    @staticmethod
    def encode(pos):
        x = int(max(-32767, min(32767, pos.x * 18204.0)))
        y = int(max(-32767, min(32767, pos.y * 18204.0)))
        z = int(max(-32767, min(32767, pos.z * 18204.0)))
        return [x, y, z]
    
    @staticmethod
    def decode(encoded):
        return Vec3(encoded[0]/18204.0, encoded[1]/18204.0, encoded[2]/18204.0)

class NormalEncoder:
    @staticmethod
    def encode(normal):
        normal = normal.normalize()
        x = int(max(0, min(255, (normal.x * 0.5 + 0.5) * 255)))
        y = int(max(0, min(255, (normal.y * 0.5 + 0.5) * 255)))
        z = int(max(0, min(255, (normal.z * 0.5 + 0.5) * 255)))
        return [x, y, z]

class SurfaceEquationFitter:
    """Fits different surface equation types to face geometry"""
    
    @staticmethod
    def fit_best_equation(positions, center, scale):
        """Try multiple equation types, return best fit"""
        plane_error = SurfaceEquationFitter.fit_plane(positions)
        sphere_error = SurfaceEquationFitter.fit_sphere(positions, center, scale)
        cylinder_error = SurfaceEquationFitter.fit_cylinder(positions, center, scale)
        
        errors = [
            (EQUATION_PLANE, plane_error[0], plane_error[1]),
            (EQUATION_SPHERE, sphere_error[0], sphere_error[1]),
            (EQUATION_CYLINDER, cylinder_error[0], cylinder_error[1])
        ]
        
        best = min(errors, key=lambda x: x[1])
        return best[0], best[2]
    
    @staticmethod
    def fit_plane(positions):
        """Fit plane: ax + by + cz + d = 0"""
        if len(positions) < 3:
            return float('inf'), [0, 0, 1, 0, 0, 0, 0, 0]
        
        positions = [p if isinstance(p, Vec3) else Vec3(p[0], p[1], p[2]) for p in positions]
        centroid = sum(positions, Vec3()) / len(positions)
        
        # PCA for plane normal
        cov = np.zeros((3, 3))
        for p in positions:
            rel = np.array([p.x - centroid.x, p.y - centroid.y, p.z - centroid.z])
            cov += np.outer(rel, rel)
        
        eigenvalues, eigenvectors = np.linalg.eig(cov)
        min_idx = np.argmin(eigenvalues)
        normal_arr = eigenvectors[:, min_idx].real
        normal = Vec3(*normal_arr).normalize()
        
        d = -(normal.x * centroid.x + normal.y * centroid.y + normal.z * centroid.z)
        
        error = sum(abs(normal.x*p.x + normal.y*p.y + normal.z*p.z + d) for p in positions)
        params = [normal.x, normal.y, normal.z, d, 0, 0, 0, 0]
        
        return error, params
    
    @staticmethod
    def fit_sphere(positions, center, scale):
        """Fit sphere: (x-cx)^2 + (y-cy)^2 + (z-cz)^2 = r^2"""
        if len(positions) < 4:
            return float('inf'), [0, 0, 1, 0, 0, 0, 0, 0]
        
        positions = [p if isinstance(p, Vec3) else Vec3(p[0], p[1], p[2]) for p in positions]
        
        # Fit sphere using least squares
        A = []
        b = []
        for p in positions:
            A.append([2*p.x, 2*p.y, 2*p.z, 1])
            b.append(p.x**2 + p.y**2 + p.z**2)
        
        try:
            A = np.array(A)
            b = np.array(b)
            x = np.linalg.lstsq(A, b, rcond=None)[0]
            
            sphere_center = Vec3(x[0], x[1], x[2])
            radius = math.sqrt(x[3] + x[0]**2 + x[1]**2 + x[2]**2)
            
            # Check if center is within bounds
            if abs(sphere_center.x) > 1.8 or abs(sphere_center.y) > 1.8 or abs(sphere_center.z) > 1.8:
                return float('inf'), [0, 0, 1, 0, 0, 0, 0, 0]
            
            error = sum((p.distance_to(sphere_center) - radius)**2 for p in positions)
            params = [sphere_center.x, sphere_center.y, sphere_center.z, radius, 0, 0, 0, 0]
            
            return error, params
        except:
            return float('inf'), [0, 0, 1, 0, 0, 0, 0, 0]
    
    @staticmethod
    def fit_cylinder(positions, center, scale):
        """Fit cylinder: (x-cx)^2 + (y-cy)^2 = r^2"""
        if len(positions) < 4:
            return float('inf'), [0, 0, 1, 0, 0, 0, 0, 0]
        
        positions = [p if isinstance(p, Vec3) else Vec3(p[0], p[1], p[2]) for p in positions]
        
        try:
            # Project to XY plane
            A = []
            b = []
            for p in positions:
                A.append([2*p.x, 2*p.y, 1])
                b.append(p.x**2 + p.y**2)
            
            A = np.array(A)
            b = np.array(b)
            x = np.linalg.lstsq(A, b, rcond=None)[0]
            
            cyl_center_x = x[0]
            cyl_center_y = x[1]
            radius = math.sqrt(x[2] + x[0]**2 + x[1]**2)
            
            error = sum((Vec3(p.x - cyl_center_x, p.y - cyl_center_y, 0).length() - radius)**2 
                       for p in positions)
            params = [cyl_center_x, cyl_center_y, 0, radius, 0, 0, 0, 0]
            
            return error, params
        except:
            return float('inf'), [0, 0, 1, 0, 0, 0, 0, 0]

class SeamDetector:
    """Detects UV seams and hard edges to prevent incorrect merges"""
    
    @staticmethod
    def detect_seams(bm, face_a, face_b):
        """Check if edge between faces is a seam (hard edge, UV seam, etc)"""
        # Find shared edge
        shared_edge = None
        for edge in face_a.edges:
            if edge in face_b.edges:
                shared_edge = edge
                break
        
        if not shared_edge:
            return True
        
        # Hard edge detection
        if shared_edge.smooth is False:
            return True
        
        # Large normal discontinuity
        normal_a = Vec3(face_a.normal[0], face_a.normal[1], face_a.normal[2])
        normal_b = Vec3(face_b.normal[0], face_b.normal[1], face_b.normal[2])
        dot = max(-1, min(1, normal_a.dot(normal_b)))
        angle = math.acos(dot) * 180.0 / math.pi
        
        if angle > 90:
            return True
        
        # UV seam detection - simplified to avoid layer access issues
        try:
            uv_layers = [l for l in bm.loops.layers.uv.values()] if bm.loops.layers.uv else []
            if uv_layers:
                uv_layer = uv_layers[0]  # Use first UV layer
                uv_a_list = []
                uv_b_list = []
                
                for loop in shared_edge.link_loops:
                    if loop.face == face_a:
                        uv_a_list.append(Vector(loop[uv_layer].uv))
                    elif loop.face == face_b:
                        uv_b_list.append(Vector(loop[uv_layer].uv))
                
                # Check if UV coordinates differ significantly
                if uv_a_list and uv_b_list:
                    dist = (uv_a_list[0] - uv_b_list[0]).length
                    if dist > 0.1:
                        return True
        except:
            pass  # Ignore UV seam detection if it fails
        
        return False

class SkeletonExtractor:
    """Extracts bone data from Blender armatures"""
    
    def __init__(self, center, scale, verbose=True):
        self.center = center
        self.scale = scale
        self.verbose = verbose
        self.bones = []
        self.bone_map = {}
    
    def extract_from_object(self, obj):
        """Extract skeleton from object's armature modifier"""
        # Find armature modifier
        armature_obj = None
        for mod in obj.modifiers:
            if mod.type == 'ARMATURE' and mod.object:
                armature_obj = mod.object
                break
        
        if not armature_obj or armature_obj.type != 'ARMATURE':
            if self.verbose:
                print("No armature found on object")
            return False
        
        return self.extract_from_armature(armature_obj)
    
    def extract_from_armature(self, armature_obj):
        """Extract bone hierarchy from armature"""
        armature_data = armature_obj.data
        
        if not armature_data.bones:
            return False
        
        if self.verbose:
            print(f"Extracting {len(armature_data.bones)} bones from {armature_obj.name}")
        
        # Build bone index map
        for idx, bone in enumerate(armature_data.bones):
            self.bone_map[bone.name] = idx
        
        # Extract bone transforms and hierarchy
        for bone in armature_data.bones:
            bone_buf = BoneBuffer()
            
            # Get world position (head of bone)
            world_pos = armature_obj.matrix_world @ bone.head
            psf_pos = self._world_to_psf(Vec3(*world_pos))
            bone_buf.pos_xyz = PositionEncoder.encode(psf_pos)
            
            # Parent relationship
            if bone.parent:
                bone_buf.parent_bone_index = self.bone_map.get(bone.parent.name, 0)
            else:
                bone_buf.parent_bone_index = len(self.bones)  # Self-reference for root
            
            # Find first child
            children = [b for b in armature_data.bones if b.parent == bone]
            if children:
                bone_buf.child_bone_index = self.bone_map[children[0].name]
            else:
                bone_buf.child_bone_index = len(self.bones)
            
            self.bones.append(bone_buf)
            
            if self.verbose:
                parent_name = bone.parent.name if bone.parent else "ROOT"
                print(f"  Bone: {bone.name} (parent: {parent_name}, index: {len(self.bones)-1})")
        
        return True
    
    def _world_to_psf(self, pos):
        """Transform world position to PSF coordinates"""
        centered = pos - self.center
        scaled = centered * self.scale
        return scaled.clamp(-1.8, 1.8)
    
    def link_vertices_to_bones(self, obj, anchor_list):
        """Link anchor points to nearest bones using vertex groups"""
        # Find armature modifier
        armature_obj = None
        for mod in obj.modifiers:
            if mod.type == 'ARMATURE' and mod.object:
                armature_obj = mod.object
                break
        
        if not armature_obj:
            return
        
        vertex_groups = obj.vertex_groups
        
        for anchor_idx, anchor in enumerate(anchor_list):
            best_bone = 0
            best_weight = 0.0
            
            # Find vertex group weights for this vertex
            vert = obj.data.vertices[anchor_idx] if anchor_idx < len(obj.data.vertices) else None
            if vert:
                for group in vert.groups:
                    group_name = vertex_groups[group.group].name
                    if group_name in self.bone_map:
                        if group.weight > best_weight:
                            best_weight = group.weight
                            best_bone = self.bone_map[group_name]
            
            anchor.bone_index = best_bone

class ColorEquationFitter:
    @staticmethod
    def fit_channel(target_values):
        """Fit best color equation to channel values at 4 corners"""
        best_error = float('inf')
        best_eqn = ColorEqn.CONSTANT
        best_param = 128
        
        uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
        
        # CONSTANT
        avg = sum(target_values) / len(target_values)
        param = max(0, min(255, int(avg * 255)))
        error = sum((param/255.0 - t)**2 for t in target_values)
        if error < best_error:
            best_error, best_eqn, best_param = error, ColorEqn.CONSTANT, param
        
        # LINEAR_U
        if target_values[1] > 0.01:
            param = max(0, min(255, int(target_values[1] * 255)))
            error = sum(((param/255.0) * u - t)**2 for (u, v), t in zip(uvs, target_values))
            if error < best_error:
                best_error, best_eqn, best_param = error, ColorEqn.LINEAR_U, param
        
        # LINEAR_V
        if target_values[3] > 0.01:
            param = max(0, min(255, int(target_values[3] * 255)))
            error = sum(((param/255.0) * v - t)**2 for (u, v), t in zip(uvs, target_values))
            if error < best_error:
                best_error, best_eqn, best_param = error, ColorEqn.LINEAR_V, param
        
        # BILINEAR
        if target_values[2] > 0.01:
            param = max(0, min(255, int(target_values[2] * 255)))
            error = sum(((param/255.0) * u * v - t)**2 for (u, v), t in zip(uvs, target_values))
            if error < best_error:
                best_error, best_eqn, best_param = error, ColorEqn.BILINEAR, param
        
        best_param = max(10, best_param)
        return best_eqn, best_param
    
    @staticmethod
    def fit_surface(corner_colors):
        """Fit color equations for RGB"""
        r_vals = [c[0] for c in corner_colors]
        g_vals = [c[1] for c in corner_colors]
        b_vals = [c[2] for c in corner_colors]
        
        eqn_r, param_r = ColorEquationFitter.fit_channel(r_vals)
        eqn_g, param_g = ColorEquationFitter.fit_channel(g_vals)
        eqn_b, param_b = ColorEquationFitter.fit_channel(b_vals)
        
        return (eqn_r, param_r), (eqn_g, param_g), (eqn_b, param_b)

class SurfaceGrouper:
    """Groups coplanar faces into large patches, respecting seams"""
    
    def __init__(self, angle_threshold=5.0, plane_tolerance=0.01, color_tolerance=0.2):
        self.angle_threshold = angle_threshold
        self.plane_tolerance = plane_tolerance
        self.color_tolerance = color_tolerance
    
    def group_faces(self, bm):
        """Group faces into surface patches"""
        used = set()
        groups = []
        
        for face in bm.faces:
            if face.index in used:
                continue
            
            group = {face.index}
            used.add(face.index)
            queue = [face]
            
            while queue:
                current = queue.pop(0)
                current_normal = Vec3(*current.normal)
                current_color = self._get_face_color(current, bm)
                
                for edge in current.edges:
                    for adjacent_face in edge.link_faces:
                        if adjacent_face.index in used:
                            continue
                        
                        # Check for seam
                        if SeamDetector.detect_seams(bm, current, adjacent_face):
                            continue
                        
                        adj_normal = Vec3(*adjacent_face.normal)
                        adj_color = self._get_face_color(adjacent_face, bm)
                        
                        if self._normals_compatible(current_normal, adj_normal) and \
                           self._colors_compatible(current_color, adj_color):
                            
                            group.add(adjacent_face.index)
                            used.add(adjacent_face.index)
                            queue.append(adjacent_face)
            
            groups.append(group)
        
        return groups
    
    def _get_face_color(self, face, bm):
        color = Vec3()
        for vert in face.verts:
            if len(vert.link_loops) > 0:
                loop = vert.link_loops[0]
                if bm.loops.layers.color:
                    vcol = loop[bm.loops.layers.color]
                    color = color + Vec3(vcol[0], vcol[1], vcol[2])
        color = color * (1.0 / len(face.verts))
        return color
    
    def _normals_compatible(self, n1, n2):
        dot = max(-1, min(1, n1.dot(n2)))
        angle = math.acos(dot) * 180.0 / math.pi
        return angle < self.angle_threshold
    
    def _colors_compatible(self, c1, c2):
        dist = math.sqrt((c1.x-c2.x)**2 + (c1.y-c2.y)**2 + (c1.z-c2.z)**2)
        return dist < self.color_tolerance

class PSFExporter:
    def __init__(self, filepath, verbose=True, angle_threshold=5.0, 
                 plane_tolerance=0.01, color_tolerance=0.2):
        self.filepath = filepath
        self.verbose = verbose
        self.angle_threshold = angle_threshold
        self.plane_tolerance = plane_tolerance
        self.color_tolerance = color_tolerance
        self.anchors = []
        self.surfaces = []
        self.bones = []
        self.anchor_map = {}
    
    def export(self, obj):
        """Export selected object to PSF"""
        if obj.type != 'MESH':
            print(f"Error: {obj.name} is not a mesh object")
            return False
        
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode='EDIT')
        bm = bmesh.from_edit_mesh(obj.data)
        bm.faces.ensure_lookup_table()
        
        try:
            # Calculate transformation
            bbox_min, bbox_max = self._calculate_bounds(obj)
            center, scale = self._calculate_transform(bbox_min, bbox_max)
            
            if self.verbose:
                print(f"Bounds: {bbox_min} to {bbox_max}")
                print(f"Center: {center}, Scale: {scale}")
            
            # Extract vertex data
            vert_positions = {}
            vert_colors = {}
            vert_normals = {}
            vertex_index_map = {}
            
            for vert in bm.verts:
                world_pos = obj.matrix_world @ vert.co
                psf_pos = self._world_to_psf(Vec3(*world_pos), center, scale)
                vert_positions[vert.index] = psf_pos
                vertex_index_map[vert.index] = vert.index
                
                normal = vert.normal
                vert_normals[vert.index] = Vec3(normal.x, normal.y, normal.z)
                
                if bm.loops.layers.color:
                    loop = vert.link_loops[0]
                    vcol = loop[bm.loops.layers.color]
                    vert_colors[vert.index] = Vec3(vcol[0], vcol[1], vcol[2])
                else:
                    vert_colors[vert.index] = Vec3(0.7, 0.5, 0.3)
            
            # Extract skeleton if present
            skeleton_extractor = SkeletonExtractor(center, scale, self.verbose)
            if skeleton_extractor.extract_from_object(obj):
                self.bones = skeleton_extractor.bones
                if self.verbose:
                    print(f"Extracted {len(self.bones)} bones")
            
            # Group faces into patches
            grouper = SurfaceGrouper(self.angle_threshold, self.plane_tolerance, 
                                     self.color_tolerance)
            face_groups = grouper.group_faces(bm)
            
            if self.verbose:
                print(f"Grouped {len(bm.faces)} faces into {len(face_groups)} patches")
            
            # Process each group
            for group_idx, face_group in enumerate(face_groups):
                self._process_surface_group(bm, face_group, vert_positions, 
                                          vert_colors, vert_normals, center, scale)
            
            if self.verbose:
                print(f"Created {len(self.anchors)} anchors, {len(self.surfaces)} surfaces")
            
            # Link vertices to bones if skeleton exists
            if self.bones:
                skeleton_extractor.link_vertices_to_bones(obj, self.anchors)
            
            return self._write_psf_file()
        
        finally:
            bpy.ops.object.mode_set(mode='OBJECT')
    
    def _calculate_bounds(self, obj):
        mesh = obj.data
        if not mesh.vertices:
            return Vec3(), Vec3()
        
        first_vert = obj.matrix_world @ mesh.vertices[0].co
        min_pos = Vec3(first_vert[0], first_vert[1], first_vert[2])
        max_pos = Vec3(first_vert[0], first_vert[1], first_vert[2])
        
        for vert in mesh.vertices:
            world_pos = obj.matrix_world @ vert.co
            pos = Vec3(world_pos[0], world_pos[1], world_pos[2])
            min_pos.x = min(min_pos.x, pos.x)
            min_pos.y = min(min_pos.y, pos.y)
            min_pos.z = min(min_pos.z, pos.z)
            max_pos.x = max(max_pos.x, pos.x)
            max_pos.y = max(max_pos.y, pos.y)
            max_pos.z = max(max_pos.z, pos.z)
        
        return min_pos, max_pos
    
    def _calculate_transform(self, min_pos, max_pos):
        center = Vec3(
            (min_pos.x + max_pos.x) * 0.5,
            (min_pos.y + max_pos.y) * 0.5,
            (min_pos.z + max_pos.z) * 0.5
        )
        
        size = Vec3(
            max_pos.x - min_pos.x,
            max_pos.y - min_pos.y,
            max_pos.z - min_pos.z
        )
        
        max_dim = max(size.x, size.y, size.z)
        if max_dim < 1e-6:
            return center, 1.0
        
        scale = 3.6 / max_dim * 0.9
        return center, scale
    
    def _world_to_psf(self, pos, center, scale):
        centered = pos - center
        scaled = centered * scale
        return scaled.clamp(-1.8, 1.8)
    
    def _process_surface_group(self, bm, face_group, vert_positions, 
                              vert_colors, vert_normals, center, scale):
        """Convert face group into surface patches"""
        if not face_group:
            return
        
        unique_verts = set()
        faces_list = []
        
        for face_idx in face_group:
            face = bm.faces[face_idx]
            faces_list.append(face)
            for vert in face.verts:
                unique_verts.add(vert.index)
        
        # Create anchors for unique vertices
        anchor_indices_map = {}
        for vert_idx in unique_verts:
            if vert_idx not in self.anchor_map:
                anchor = AnchorPointBuffer()
                anchor.pos_xyz = PositionEncoder.encode(vert_positions[vert_idx])
                anchor.normal = NormalEncoder.encode(vert_normals[vert_idx])
                
                color = vert_colors[vert_idx]
                anchor.color = [
                    max(0, min(255, int(color.x * 255))),
                    max(0, min(255, int(color.y * 255))),
                    max(0, min(255, int(color.z * 255)))
                ]
                
                self.anchor_map[vert_idx] = len(self.anchors)
                self.anchors.append(anchor)
            
            anchor_indices_map[vert_idx] = self.anchor_map[vert_idx]
        
        # Process each face in group
        for face in faces_list:
            if len(face.verts) < 3:
                continue
            
            # Convert face to quad
            vert_indices = [v.index for v in face.verts[:4]]
            if len(vert_indices) == 3:
                vert_indices.append(vert_indices[-1])
            
            anchor_indices = [anchor_indices_map[vi] for vi in vert_indices]
            
            # Create surface
            surface = ProceduralSurfaceBuffer()
            surface.anchor_point_indices = anchor_indices
            
            # Fit best surface equation
            positions = [vert_positions[vi] for vi in vert_indices[:3]]
            eqn_type, params = SurfaceEquationFitter.fit_best_equation(positions, center, scale)
            surface.surface_eqn = eqn_type
            surface.params = params
            
            # Fit color equations
            corner_colors = [vert_colors[vi] for vi in vert_indices]
            corner_colors = [(c.x, c.y, c.z) for c in corner_colors]
            
            (eqn_r, param_r), (eqn_g, param_g), (eqn_b, param_b) = \
                ColorEquationFitter.fit_surface(corner_colors)
            
            surface.color_eqn_r = eqn_r
            surface.color_eqn_g = eqn_g
            surface.color_eqn_b = eqn_b
            surface.color_param_r = param_r
            surface.color_param_g = param_g
            surface.color_param_b = param_b
            
            self.surfaces.append(surface)
    
    def _write_psf_file(self) -> bool:
        """Write PSF format file"""
        try:
            with open(self.filepath, 'wb') as f:
                # Header (256 bytes)
                f.write(struct.pack('<I', PSF_MAGIC))
                f.write(struct.pack('<I', PSF_VERSION))
                name_padding = b'PSF_EXPORT\0' + b'\0' * 245
                f.write(name_padding[:256])
                
                # Pool sizes
                f.write(struct.pack('<I', 1))  # object count
                f.write(struct.pack('<I', len(self.anchors)))
                f.write(struct.pack('<I', len(self.surfaces)))
                f.write(struct.pack('<I', len(self.bones)))
                
                # Anchors
                if self.verbose:
                    print(f"Writing {len(self.anchors)} anchors...")
                for i, anchor in enumerate(self.anchors):
                    try:
                        data = anchor.pack()
                        if len(data) != 22:
                            print(f"ERROR: Anchor {i} packed to {len(data)} bytes, expected 22")
                            return False
                        f.write(data)
                    except Exception as e:
                        print(f"ERROR packing anchor {i}: {e}")
                        return False
                
                # Surfaces
                if self.verbose:
                    print(f"Writing {len(self.surfaces)} surfaces...")
                for i, surface in enumerate(self.surfaces):
                    try:
                        data = surface.pack()
                        if len(data) != 60:
                            print(f"ERROR: Surface {i} packed to {len(data)} bytes, expected 60")
                            return False
                        f.write(data)
                    except Exception as e:
                        print(f"ERROR packing surface {i}: {e}")
                        return False
                
                # Bones
                if self.verbose:
                    print(f"Writing {len(self.bones)} bones...")
                for i, bone in enumerate(self.bones):
                    try:
                        data = bone.pack()
                        if len(data) != 30:
                            print(f"ERROR: Bone {i} packed to {len(data)} bytes, expected 30")
                            return False
                        f.write(data)
                    except Exception as e:
                        print(f"ERROR packing bone {i}: {e}")
                        return False
            
            if self.verbose:
                print(f"✓ Successfully wrote PSF: {self.filepath}")
                print(f"  Anchors: {len(self.anchors)}")
                print(f"  Surfaces: {len(self.surfaces)}")
                print(f"  Bones: {len(self.bones)}")
            return True
        
        except Exception as e:
            print(f"✗ Error writing PSF: {e}")
            import traceback
            traceback.print_exc()
            return False

class PSF_OT_export(bpy.types.Operator):
    bl_idname = "export.psf_format"
    bl_label = "Export PSF Format"
    bl_options = {'REGISTER', 'UNDO'}
    
    filepath: bpy.props.StringProperty(
        name="File Path",
        description="Filepath for PSF export",
        subtype='FILE_PATH'
    )
    
    filter_glob: bpy.props.StringProperty(
        default="*.psf",
        options={'HIDDEN'}
    )
    
    angle_threshold: bpy.props.FloatProperty(
        name="Normal Angle Threshold",
        description="Max angle difference for surfaces to merge (degrees)",
        default=5.0, min=0.1, max=90.0, step=0.1
    )
    
    plane_tolerance: bpy.props.FloatProperty(
        name="Plane Distance Tolerance",
        description="Max distance for surfaces to merge",
        default=0.01, min=0.001, max=1.0, step=0.001
    )
    
    color_tolerance: bpy.props.FloatProperty(
        name="Color Tolerance",
        description="Max color difference to merge (0-1)",
        default=0.2, min=0.01, max=1.0, step=0.01
    )
    
    use_seam_detection: bpy.props.BoolProperty(
        name="Detect Seams",
        description="Prevent merging across UV/hard edge seams",
        default=True
    )
    
    use_curved_surfaces: bpy.props.BoolProperty(
        name="Use Curved Surfaces",
        description="Fit spheres and cylinders in addition to planes",
        default=True
    )
    
    extract_skeleton: bpy.props.BoolProperty(
        name="Extract Skeleton",
        description="Extract bones from armature modifier",
        default=True
    )
    
    verbose: bpy.props.BoolProperty(
        name="Verbose Output",
        description="Print detailed export information",
        default=True
    )
    
    def execute(self, context):
        if not self.filepath:
            self.report({'ERROR'}, "Filepath not specified")
            return {'CANCELLED'}
        
        if not context.selected_objects:
            self.report({'ERROR'}, "No objects selected")
            return {'CANCELLED'}
        
        obj = context.selected_objects[0]
        
        exporter = PSFExporter(
            self.filepath,
            verbose=self.verbose,
            angle_threshold=self.angle_threshold,
            plane_tolerance=self.plane_tolerance,
            color_tolerance=self.color_tolerance
        )
        
        if exporter.export(obj):
            self.report({'INFO'}, f"Exported to {self.filepath}")
            return {'FINISHED'}
        else:
            self.report({'ERROR'}, "Export failed - see console")
            return {'CANCELLED'}
    
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}
    
    def draw(self, context):
        layout = self.layout
        layout.prop(self, "angle_threshold")
        layout.prop(self, "plane_tolerance")
        layout.prop(self, "color_tolerance")
        layout.prop(self, "use_seam_detection")
        layout.prop(self, "use_curved_surfaces")
        layout.prop(self, "extract_skeleton")
        layout.prop(self, "verbose")

class PSF_PT_panel(bpy.types.Panel):
    bl_label = "PSF Format Exporter"
    bl_idname = "VIEW_3D_PT_psf_exporter"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'PSF'
    
    def draw(self, context):
        layout = self.layout
        layout.label(text="Export Settings", icon='SETTINGS')
        
        col = layout.column()
        col.label(text="Select object and click Export")
        col.operator("export.psf_format", text="Export Selected to PSF", icon='EXPORT')
        
        layout.separator()
        layout.label(text="Features:", icon='INFO')
        col = layout.column()
        col.label(text="✓ Smart surface merging")
        col.label(text="✓ Seam detection")
        col.label(text="✓ Curved surfaces")
        col.label(text="✓ Skeleton extraction")
        col.label(text="✓ Color preservation")

def register():
    bpy.utils.register_class(PSF_OT_export)
    bpy.utils.register_class(PSF_PT_panel)
    print("✓ PSF Exporter addon registered")

def unregister():
    bpy.utils.unregister_class(PSF_OT_export)
    bpy.utils.unregister_class(PSF_PT_panel)
    print("✗ PSF Exporter addon unregistered")

if __name__ == "__main__":
    register()