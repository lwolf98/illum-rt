include(preamble.glsl)


void run(uint x, uint y) {
	uint id = y * w + x;
	int tri_id = floatBitsToInt(intersections[id].w);
	int mat_id = triangles[tri_id].w;
}
