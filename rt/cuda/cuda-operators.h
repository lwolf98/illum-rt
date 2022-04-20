#pragma once

__device__ __inline__ float2 operator*(float l, const float2 &r) { return { l*r.x, l*r.y }; }
__device__ __inline__ float2 operator*(const float2 &l, float r) { return { l.x*r, l.y*r }; }
__device__ __inline__ float2 operator*(const float2 &l, const float2 &r) { return { l.x*r.x, l.y*r.y }; }

__device__ __inline__ float2 operator+(const float2 &l, const float2 &r) { return { l.x+r.x, l.y+r.y }; }

__device__ __inline__ float4 operator+(const float4 &l, const float4 &r) { return { l.x+r.x, l.y+r.y, l.z+r.z, l.w+r.w }; }
__device__ __inline__ float4& operator+=(float4 &l, const float4 &r) { l.x+=r.x, l.y+=r.y, l.z+=r.z, l.w+=r.w; return l; }

__forceinline__ __device__ void normalize(float3 &v) {
    float inv_len = rsqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    v.x = v.x * inv_len;
    v.y = v.y * inv_len;
    v.z = v.z * inv_len;
}

__forceinline__ __device__ __host__ float dot(const float3 &a, const float3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__forceinline__ __device__ __host__ void cross(float3 &dest, const float3 &a, const float3 &b) {
    dest.x = a.y*b.z - a.z*b.y;
    dest.y = a.z*b.x - a.x*b.z;
    dest.z =  a.x*b.y - a.y*b.x;
}

__forceinline__  __device__ __host__ float3 cross(const float3 &a, const float3 &b) {
    return make_float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}


