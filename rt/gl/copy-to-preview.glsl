include(preamble.glsl)

uniform layout(rgba32f,binding=2) image2D framebuffer;

void run(uint x, uint y) {
	preview_framebuffer[y*w+x] = imageLoad(framebuffer, ivec2(x, y));
}