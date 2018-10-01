import sys, re

args = []
for i in range(0, len(sys.argv)):
	if sys.argv[i] == '--':
		args = sys.argv[i+1:]


if len(args) != 2:
	print("\n\nUsage:\nblender --background --python export-walkmesh.py -- <infile.blend>[:layer] <outfile.p[n][c][t][l]>\nExports the walking mesh in layer (default 1) to a binary blob, indexed by the names of the objects that reference them. It will exported by all the vertices and the index of triangles following them\n")
	exit(1)


infile = args[0]
layer = 1
m = re.match(r'^(.*):(\d+)$', infile)

if m:
	infile = m.group(1)
	layer = int(m.group(2))

outfile = args[1]

assert layer >= 1 and layer <= 20



import bpy
import struct

bpy.ops.wm.open_mainfile(filepath=infile)

to_write = set()
for obj in bpy.data.objects:
	if obj.layers[layer-1] and obj.type == 'MESH':
		to_write.add(obj.data)

#data contains vertex and normal data from the meshes:
vertex_data = b''
triangle_data = b''

strings = b''

index = b''

vertex_count = 0

for obj in bpy.data.objects:
	if obj.data in to_write:
		to_write.remove(obj.data)
	else:
		continue

	mesh = obj.data
	name = mesh.name
	print("Writing '" + name + "'...")
	if bpy.context.mode == 'EDIT':
		bpy.ops.object.mode_set(mode='OBJECT')

	bpy.context.scene.layers = obj.layers

	bpy.ops.object.select_all(action='DESELECT')
	obj.select = True

	bpy.context.scene.objects.active = obj

	bpy.ops.object.convert(target='MESH')

	bpy.ops.object.mode_set(mode='EDIT')
	bpy.ops.mesh.select_all(action='SELECT')
	bpy.ops.mesh.quads_convert_to_tris(quad_method='BEAUTY', ngon_method='BEAUTY')
	bpy.ops.object.mode_set(mode='OBJECT')

	mesh.calc_normals()

	name_begin = len(strings)
	strings += bytes(name, "utf8")
	name_end = len(strings)

	index += struct.pack("I", name_begin)
	index += struct.pack("I", name_end)

	index += struct.pack('I', vertex_count)

	for vertex in mesh.vertices:
		for x in vertex.co:
			vertex_data += struct.pack('f', x)

		vertex_data += struct.pack('f', vertex.normal[0])
		vertex_data += struct.pack('f', vertex.normal[1])
		vertex_data += struct.pack('f', vertex.normal[2])


	for poly in mesh.polygons:
		for vertex_index in poly.vertices:
			triangle_data += struct.pack('I', vertex_index)


blob = open(outfile, 'wb')

blob.write(struct.pack('4s', b'vex0'))
blob.write(struct.pack('I', len(vertex_data)))
blob.write(vertex_data)

blob.write(struct.pack('4s', b'tri0'))
blob.write(struct.pack('I', len(triangle_data)))
blob.write(triangle_data)

wrote = blob.tell()
blob.close()


print("Wrote " + str(wrote) + " bytes [== " + str(len(vertex_data)+8) + " bytes of data + " + str(len(strings)+8) + " bytes of strings + " + str(len(index)+8) + " bytes of index] to '" + outfile + "'")



