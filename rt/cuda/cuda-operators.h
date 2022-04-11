__device__ __inline__ float2 operator*(float l, const float2 &r) { return { l*r.x, l*r.y }; }
__device__ __inline__ float2 operator*(const float2 &l, float r) { return { l.x*r, l.y*r }; }
__device__ __inline__ float2 operator*(const float2 &l, const float2 &r) { return { l.x*r.x, l.y*r.y }; }

__device__ __inline__ float2 operator+(const float2 &l, const float2 &r) { return { l.x+r.x, l.y+r.y }; }
