include(preamble.glsl)

uniform layout(rgba32f,binding=2) image2D framebuffer;

void run(uint x, uint y) {
	imageStore(framebuffer, ivec2(x, y), vec4(0,0,0,0));
}
