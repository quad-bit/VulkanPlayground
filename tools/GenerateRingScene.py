import bpy
import random
import math
from mathutils import Vector

# --------------------------------------------------
# CONFIG
# --------------------------------------------------

NUM_OBJECTS = 1000

INNER_RADIUS = 200.0
OUTER_RADIUS = 800.0

MIN_SCALE = 0.5
MAX_SCALE = 2.5

EXPORT_PATH = r"./ring_scene.gltf"  # Change this

SEED = 42

# --------------------------------------------------
# CLEAN SCENE
# --------------------------------------------------

random.seed(SEED)

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

# Remove orphan meshes/materials
for block in bpy.data.meshes:
    if block.users == 0:
        bpy.data.meshes.remove(block)

# --------------------------------------------------
# MATERIALS (NO TEXTURES)
# --------------------------------------------------

materials = []

colors = [
    (0.9, 0.2, 0.2, 1.0),
    (0.2, 0.9, 0.2, 1.0),
    (0.2, 0.4, 0.9, 1.0),
    (0.9, 0.8, 0.2, 1.0),
]

for i, color in enumerate(colors):
    mat = bpy.data.materials.new(f"Mat_{i}")
    mat.use_nodes = True

    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Roughness"].default_value = 0.6

    materials.append(mat)

# --------------------------------------------------
# CREATE SHARED PROTOTYPES
# --------------------------------------------------

prototypes = []

# Cube
bpy.ops.mesh.primitive_cube_add(size=2)
cube = bpy.context.active_object
cube.name = "PROTO_CUBE"
cube.hide_viewport = True
cube.hide_render = True
prototypes.append(cube)

# Cone
bpy.ops.mesh.primitive_cone_add(
    radius1=1,
    depth=2,
    vertices=24
)
cone = bpy.context.active_object
cone.name = "PROTO_CONE"
cone.hide_viewport = True
cone.hide_render = True
prototypes.append(cone)

# Sphere
bpy.ops.mesh.primitive_uv_sphere_add(
    radius=1,
    segments=24,
    ring_count=12
)
sphere = bpy.context.active_object
sphere.name = "PROTO_SPHERE"
sphere.hide_viewport = True
sphere.hide_render = True
prototypes.append(sphere)

# Torus
bpy.ops.mesh.primitive_torus_add(
    major_radius=1.0,
    minor_radius=0.35,
    major_segments=32,
    minor_segments=16
)
torus = bpy.context.active_object
torus.name = "PROTO_TORUS"
torus.hide_viewport = True
torus.hide_render = True
prototypes.append(torus)

# --------------------------------------------------
# INSTANCE OBJECTS
# --------------------------------------------------

collection = bpy.context.collection

for i in range(NUM_OBJECTS):

    proto = random.choice(prototypes)

    obj = proto.copy()
    obj.data = proto.data

    collection.objects.link(obj)

    # Ring distribution
    angle = random.uniform(0, 2 * math.pi)
    radius = random.uniform(INNER_RADIUS, OUTER_RADIUS)

    x = math.cos(angle) * radius
    y = math.sin(angle) * radius
    z = random.uniform(-5.0, 5.0)

    obj.location = Vector((x, y, z))

    obj.rotation_euler = (
        random.uniform(0, 2 * math.pi),
        random.uniform(0, 2 * math.pi),
        random.uniform(0, 2 * math.pi)
    )

    s = random.uniform(MIN_SCALE, MAX_SCALE)
    obj.scale = (s, s, s)

    obj.hide_viewport = False
    obj.hide_render = False

    mat = random.choice(materials)

    if len(obj.data.materials) == 0:
        obj.data.materials.append(mat)
    else:
        obj.data.materials[0] = mat

# --------------------------------------------------
# DELETE PROTOTYPES FROM EXPORT
# --------------------------------------------------

for proto in prototypes:
    proto.hide_render = True
    proto.hide_viewport = True

# --------------------------------------------------
# EXPORT GLTF + BIN
# --------------------------------------------------

bpy.ops.export_scene.gltf(
    filepath=EXPORT_PATH,
    export_format='GLTF_SEPARATE',
    export_apply=True,
    export_texcoords=False,
    export_normals=True,
    export_tangents=False,
    export_materials='EXPORT',
    export_cameras=False,
    export_lights=False,
)

print(f"Exported: {EXPORT_PATH}")