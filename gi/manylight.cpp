#include "manylight.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"
#include "libgi/sampling.h"

#include "libgi/timer.h"

#include "libgi/global-context.h"

using namespace glm;
using namespace std;

void manylight_algorithm::prepare_frame() {
    /* Generate VPLs */

    // 1. initialize j:=0
    int j = 0;
    bool pathTerminated = false;

    // setup russian roulette
    russian_roulette rr(10);

    while(!pathTerminated) {
        // 2. sample the next path vertex

        // 3. create a VPL

        // 4. terminate path
        pathTerminated = rr.shot();

        // 5. increment j and go to step 2
        j++;
    }
}

vec3 manylight_algorithm::sample_pixel(uint32_t x, uint32_t y) {
    vec3 radiance(0);

    /* Render with VPLs */

    return radiance;
}

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