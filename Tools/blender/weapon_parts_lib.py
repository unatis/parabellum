"""
Библиотека параметрических деталей оружия для Blender. Единицы - миллиметры (1 BU = 1 мм),
экспорт пересчитывается в сантиметры Unreal. Деталь строится числами из чертежа, а не лепится руками:
модель пересобирается командой при изменении любого размера.
"""
import bpy, bmesh, math
from mathutils import Vector


def new_mesh(name):
    me = bpy.data.meshes.new(name)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def profile_extrude(name, pts_xz, width, y0=None):
    """Профиль (вид сбоку) в плоскости XZ, выдавленный по Y на width. pts_xz - список (x, z) мм."""
    ob = new_mesh(name)
    bm = bmesh.new()
    y = y0 if y0 is not None else -width / 2.0
    lo = [bm.verts.new((x, y, z)) for (x, z) in pts_xz]
    hi = [bm.verts.new((x, y + width, z)) for (x, z) in pts_xz]
    bm.faces.new(lo[::-1])
    bm.faces.new(hi)
    n = len(pts_xz)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bm.normal_update()
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def revolve(name, profile_rz, segments=32, axis='X'):
    """Тело вращения вокруг оси. profile_rz - список (вдоль_оси, радиус) мм."""
    ob = new_mesh(name)
    bm = bmesh.new()
    rings = []
    for (a, r) in profile_rz:
        ring = []
        for i in range(segments):
            t = 2 * math.pi * i / segments
            if axis == 'X':
                ring.append(bm.verts.new((a, r * math.cos(t), r * math.sin(t))))
            else:
                ring.append(bm.verts.new((r * math.cos(t), r * math.sin(t), a)))
        rings.append(ring)
    for A, B in zip(rings[:-1], rings[1:]):
        for i in range(segments):
            j = (i + 1) % segments
            bm.faces.new((A[i], A[j], B[j], B[i]))
    bm.faces.new(rings[0][::-1])
    bm.faces.new(rings[-1])
    bm.normal_update()
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def helix(name, coil_r, wire_r, pitch, turns, segments=16, ring=8, axis_x=0.0):
    """Винтовая пружина вдоль оси X."""
    ob = new_mesh(name)
    bm = bmesh.new()
    steps = int(segments * turns)
    prev = None
    for s in range(steps + 1):
        t = 2 * math.pi * s / segments
        c = Vector((axis_x + pitch * s / segments, coil_r * math.cos(t), coil_r * math.sin(t)))
        tangent = Vector((pitch / (2 * math.pi), -coil_r * math.sin(t), coil_r * math.cos(t))).normalized()
        up = Vector((0, 0, 1)) if abs(tangent.z) < 0.9 else Vector((1, 0, 0))
        n1 = tangent.cross(up).normalized()
        n2 = tangent.cross(n1).normalized()
        cur = [bm.verts.new(c + n1 * (wire_r * math.cos(2 * math.pi * k / ring)) + n2 * (wire_r * math.sin(2 * math.pi * k / ring))) for k in range(ring)]
        if prev:
            for k in range(ring):
                m = (k + 1) % ring
                bm.faces.new((prev[k], prev[m], cur[m], cur[k]))
        prev = cur
    bm.normal_update()
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def boolean(target, cutter, op='DIFFERENCE'):
    m = target.modifiers.new("bool", 'BOOLEAN')
    m.operation = op
    m.object = cutter
    m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.data.objects.remove(cutter, do_unlink=True)
    return target


def box(name, size, center=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    ob = bpy.context.active_object
    ob.name = name
    ob.scale = (size[0], size[1], size[2])   # куб size=1 занимает ровно 1 единицу
    bpy.ops.object.transform_apply(scale=True)
    return ob


def dims(ob):
    return tuple(round(v, 1) for v in ob.dimensions)


def move(ob, v):
    """Сдвиг геометрии без операторов (надёжнее, чем location + transform_apply)."""
    from mathutils import Matrix
    ob.data.transform(Matrix.Translation(Vector(v)))
    return ob


def rotate(ob, angle_deg, axis='Y'):
    """Поворот геометрии вокруг оси, проходящей через ноль. Тоже без операторов."""
    from mathutils import Matrix
    ob.data.transform(Matrix.Rotation(math.radians(angle_deg), 4, axis))
    return ob


def place(ob, angle_deg, offset, axis='Y'):
    """Деталь построена в своей системе координат: повернуть и поставить на место."""
    rotate(ob, angle_deg, axis)
    return move(ob, offset)


def rrect_ring(cx, cy, z, w, d, r, n=6):
    """Кольцо-скруглённый прямоугольник в плоскости XY на высоте z. w - вдоль X, d - вдоль Y."""
    pts = []
    hw, hd = w / 2 - r, d / 2 - r
    for (sx, sy, a0) in ((1, 1, 0.0), (-1, 1, math.pi / 2), (-1, -1, math.pi), (1, -1, 1.5 * math.pi)):
        for i in range(n):
            a = a0 + (math.pi / 2) * i / (n - 1)
            pts.append((cx + sx * hw + r * math.cos(a) * (1 if sx > 0 else -1) * (1 if abs(math.cos(a)) else 1),
                        cy + sy * hd + r * math.sin(a) * (1 if sy > 0 else -1), z))
    # проще и устойчивее: параметрическое суперэллиптическое кольцо
    pts = []
    total = 4 * n
    for i in range(total):
        t = 2 * math.pi * i / total
        ct, st = math.cos(t), math.sin(t)
        k = 4.0   # степень суперэллипса: чем больше, тем прямоугольнее
        x = cx + (w / 2) * math.copysign(abs(ct) ** (2.0 / k), ct)
        y = cy + (d / 2) * math.copysign(abs(st) ** (2.0 / k), st)
        pts.append((x, y, z))
    return pts


def loft(name, rings, cap=True):
    """Оболочка по набору колец (каждое - список точек одинаковой длины)."""
    ob = new_mesh(name)
    bm = bmesh.new()
    vs = [[bm.verts.new(p) for p in ring] for ring in rings]
    n = len(rings[0])
    for A, B in zip(vs[:-1], vs[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((A[i], A[j], B[j], B[i]))
    if cap:
        bm.faces.new(vs[0][::-1])
        bm.faces.new(vs[-1])
    bm.normal_update()
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def bevel(ob, width=0.4, segments=2, angle_deg=35.0):
    """Фаски по рёбрам: без них металл и пластик читаются как картон - блик по кромке даёт 'дорогой' вид."""
    m = ob.modifiers.new("bevel", 'BEVEL')
    m.width = width
    m.segments = segments
    m.limit_method = 'ANGLE'
    m.angle_limit = math.radians(angle_deg)
    m.use_clamp_overlap = True
    m.harden_normals = False
    bpy.context.view_layer.objects.active = ob
    try:
        bpy.ops.object.modifier_apply(modifier=m.name)
    except RuntimeError:
        ob.modifiers.remove(m)
    return ob


def shade_smooth(ob, angle_deg=30.0):
    """Сглаживание по углу: округлости гладкие, плоские грани остаются плоскими."""
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    try:
        bpy.ops.object.shade_auto_smooth(angle=math.radians(angle_deg))
    except Exception:
        ob.data.polygons.foreach_set("use_smooth", [False] * len(ob.data.polygons))
    ob.select_set(False)
    return ob


def material(name, base_color, metallic, roughness):
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*base_color, 1.0)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = roughness
    m.diffuse_color = (*base_color, 1.0)
    m.metallic = metallic
    m.roughness = roughness
    return m


def assign(ob, mat):
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    return ob
