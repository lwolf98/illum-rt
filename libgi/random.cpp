#include "random.h"

#include <omp.h>

rng_std_mt::rng_std_mt() : per_thread_rng(omp_get_max_threads()) {
	for (int i = 0; i < omp_get_max_threads(); ++i)
		per_thread_rng[i].seed(i);
}
	
float rng_std_mt::uniform_float() const {
	// the C++ standard has a bug, where 1.f can be returned with float
	float r = uniform_float_distribution(per_thread_rng[omp_get_thread_num()]);
	return r < 1.0f ? r : 0.99999f;
}
    
uint32_t rng_std_mt::uniform_uint() const {
	return uniform_uint_distribution(per_thread_rng[omp_get_thread_num()]);
}
 


vec2 rng_std_mt::uniform_float2() const {
	return {uniform_float(), uniform_float()};
}
 
