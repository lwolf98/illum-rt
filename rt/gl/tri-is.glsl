bool intersect(vec4 tri_a, vec4 tri_b, vec4 tri_c, vec4 ray_o, vec4 ray_d, vec2 t_range, out vec4 info) {
	const float a_x = tri_a.x;
	const float a_y = tri_a.y;
	const float a_z = tri_a.z;

	const float a = a_x - tri_b.x;
	const float b = a_y - tri_b.y;
	const float c = a_z - tri_b.z;

	const float d = a_x - tri_c.x;
	const float e = a_y - tri_c.y;
	const float f = a_z - tri_c.z;

	const float g = ray_d.x;
	const float h = ray_d.y;
	const float i = ray_d.z;

	const float j = a_x - ray_o.x;
	const float k = a_y - ray_o.y;
	const float l = a_z - ray_o.z;

	float common1 = e*i - h*f;
	float common2 = g*f - d*i;
	float common3 = d*h - e*g;
	float M 	  = a * common1  +  b * common2  +  c * common3;
	float beta 	  = j * common1  +  k * common2  +  l * common3;

	common1       = a*k - j*b;
	common2       = j*c - a*l;
	common3       = b*l - k*c;
	float gamma   = i * common1  +  h * common2  +  g * common3;
	float tt    = -(f * common1  +  e * common2  +  d * common3);

	beta /= M;
	gamma /= M;
	tt /= M;	// opt: test before by *M

	if (tt > t_range.x && tt < t_range.y)
		if (beta > 0 && gamma > 0 && beta + gamma <= 1)
		{
			info.x = tt;
			info.y = beta;
			info.z = gamma;
			return true;
		}

	return false;
}	

bool intersect(int tri_id, vec4 ray_o, vec4 ray_d, vec2 t_range, out vec4 info) {
	ivec4 tri = triangles[tri_id];
	vec4 a = vertices[tri.x].pos;
	vec4 b = vertices[tri.y].pos;
	vec4 c = vertices[tri.z].pos;
	return intersect(a, b, c, ray_o, ray_d, t_range, info);
}

