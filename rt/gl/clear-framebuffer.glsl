include(preamble.glsl)

void run(uint x, uint y) {
	framebuffer[y*w+x] = vec4(0,0,0,0);
}
