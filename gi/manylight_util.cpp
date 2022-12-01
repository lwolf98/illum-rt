#include "manylight.h"
#include "libgi/global-context.h"

/*** Util ***/
bool russian_roulette::shot() {
    if(cold_count < start) {
        cold_count++;
        return false;
    }
    if(hot_count >= max_hot) {
        return true;
    }

    float rnd = rc->rng.uniform_float();
    float p = 1.0f/(max_hot-hot_count);
    bool shot = p > rnd;

    hot_count++;
    return shot;
}

void russian_roulette::reset() {
    cold_count = 1;
    hot_count = 0;
}

vec3 random_dir() {
    float x = rc->rng.uniform_float() - 0.5f;
    float y = rc->rng.uniform_float() - 0.5f;
    float z = rc->rng.uniform_float() - 0.5f;
    return normalize(vec3(x,y,z));
}