#include "manylight.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"
#include "libgi/sampling.h"

#include "libgi/timer.h"

#include "libgi/global-context.h"

#include "gi/objdraw.h"

using namespace glm;
using namespace std;

void file_put_contents(const std::string& name, const std::string& content, bool append);
void test_paths(const camera &camera, int stride);
vec3 random_dir();

void manylight_algorithm::prepare_frame() {
    /* Generate VPLs */

    // 1. initialize j:=0
    int j = 0;
    bool pathTerminated = false;

    // setup russian roulette
	int length = 8;
    russian_roulette rr(length, length);
	int samples = 128;

	vec3 origin_pl(0,-4,0);
    //vec3 col(0.7,0.7,0.7);
    //vec3 col(10, 32, 8);
    vec3 col(3.0f);

    // test: begin paths.obj
    vector<vec3> vertices;
    ofstream out("paths.obj");
    int32_t start = 0;
	int32_t off = 0;

	for(int i = 0; i < samples; ++i) {
		objdraw::path path(origin_pl);
		vec3 pos = origin_pl;
		vec3 dir = random_dir();
        //vec3 throughput(1.0f);
        vec3 throughput(col*(1.0f/samples));

		int j = 0;
		while(!pathTerminated) {
			ray ray(pos, dir);

			// 2. sample the next path vertex
			triangle_intersection closest = rc->scene.rt->closest_hit(ray);
			if(!closest.valid()) {
				cout << "NO valid hit! sample: " << i << ", " << "iteration j = " << j << endl;
				break;
			}
			diff_geom hit(closest, rc->scene);

            // if it is a light, add the light's contribution
            /*if (hit.mat->emissive != vec3(0)) {
                //radiance = throughput * dg.mat->emissive;
                //break;
                return radiance + hit.mat->emissive;
            }*/

			auto [bounced, pdf] = sample_brdf_distributed_direction(hit, ray);
		    throughput *= hit.mat->brdf->f(hit, -ray.d, bounced.d) * cdot(bounced.d, hit.ns) / pdf;
			//cout << " (" << bounced.o << "; " << bounced.d << "; " << pdf << ")" << endl;

			// 3. create a VPL
            //if(j == 0)
            //    throughput *= col;

			pos = bounced.o;
			dir = bounced.d;
            vec3 col = hit.albedo();
			//vpl* v = new vpl(pos, col);
            vpl v(pos, col);
            v.throughput = throughput;
            vpls.push_back(v);

			path.push_vertex(pos);
			vertices.push_back(pos);

			// test: add pointlight
			//rc->scene.lights.push_back(v);
			//rc->scene.lights.resize(rc->scene.lights.size() + 1);
			//rc->scene.lights[rc->scene.lights.size() - 1] = v;

			// 4. terminate path
			pathTerminated = rr.shot();

			// 5. increment j and go to step 2
			j++;
		}
		//cout << "sample: " << i << ", terminated at: j = " << j << endl << endl;
		out << path.obj_string(start) << endl;

		rr.reset();
		pathTerminated = false;
		j = 0;
	}

	out << objdraw::icosphere(origin_pl).obj_string(start) << endl;
    for(auto v : vertices) {
        objdraw::icosphere sphere(v, 0.5f);
        out << sphere.obj_string(start) << endl;
    }

    // test: check lights
    /*int i = 0;
    for(light* l : rc->scene.lights) {
        pointlight* pl;
        trianglelight* tl;
        if(pl = dynamic_cast<pointlight*> (l))
            cout << i << ": pointlight" << endl;
        else if(tl = dynamic_cast<trianglelight*> (l)) {
            cout << i << ": trianglelight: ";
            cout << "(" << rc->scene.vertices[tl->a].pos << "), ";
            cout << "(" << rc->scene.vertices[tl->b].pos << "), ";
            cout << "(" << rc->scene.vertices[tl->c].pos << "), ";
            cout << "(" << rc->scene.materials[tl->material_id].albedo << ")" << endl;;

        } else if(dynamic_cast<skylight*> (l))
            cout << i << ": skylight" << endl;
        else
            cout << i <<": none of the above" << endl;

        i++;
    }*/
}

vec3 manylight_algorithm::sample_pixel(uint32_t x, uint32_t y) {
    vec3 radiance(0);
    int n = vpls.size();

    //if(x == 0)
    //    cout << "New row: " << y << endl;

	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (!closest.valid())
        return vec3(0);

    diff_geom dg(closest, rc->scene);
    //radiance = dg.albedo();

    // if it is a light, add the light's contribution
    if (dg.mat->emissive != vec3(0)) {
        //radiance = throughput * dg.mat->emissive;
        //break;
        return dg.mat->emissive;
    }

    /* Render with VPLs */
    int i = 0;
    for(auto v : vpls) {
        vec3 dir = normalize(dg.x - v.pos);
        triangle_intersection vpl_closest = rc->scene.rt->closest_hit(ray(v.pos, dir));
        diff_geom vpl_dg(vpl_closest, rc->scene);
        //if(vpl_dg.x != dg.x) {
        if(length(vpl_dg.x - dg.x) > 0.1f) {
        //auto tmp = vpl_dg.x - dg.x;
        //if(dot(tmp, tmp) > 0.1f*0.1f) {
            //cout << "difference: " << vpl_dg.x - dg.x << endl;
            //cout << "difference: " << length(vpl_dg.x - dg.x) << endl;
            /*if(length(vpl_dg.x - dg.x) < 0.1f) {
                //cout << "difference: " << length(vpl_dg.x - dg.x) << ", hit: " << dg.x << ", vpl_hit: " << vpl_dg.x << endl;
            } else {
                //cout << "difference: " << length(vpl_dg.x - dg.x) << ", hit: " << dg.x << ", vpl_hit: " << vpl_dg.x << endl;
            }*/

            continue;
        }
        //cout << "same hit?" << endl;
        //cout << "difference: " << length(vpl_dg.x - dg.x) << ", hit: " << dg.x << ", vpl_hit: " << vpl_dg.x << endl;

        float D = cdot(dg.ns, -dir);
        //cout << "radiance pre: " << radiance << endl;
        float pdf = dg.mat->brdf->pdf(dg, view_ray.d, -dir);
        radiance += D*v.throughput*dg.mat->brdf->f(dg, view_ray.d, -dir)*(1.0f/pdf);
        //radiance += D*v->throughput*dg.mat->brdf->f(dg, view_ray.d, -dir)*(1.0f/n);
        //cout << "add: " << D*v->throughput*v->col*(1.0f/n) << endl;
        //cout << "radiance after: " << radiance << endl;
    }

    return radiance; //*dg.albedo()*4.0f*pi;
}

manylight_algorithm::~manylight_algorithm() {
    // reference here correct? what does it do here exactly?
    /*for(auto& v : vpls)
        delete v;*/
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

vec3 random_dir() {
    float x = rc->rng.uniform_float() - 0.5f;
    float y = rc->rng.uniform_float() - 0.5f;
    float z = rc->rng.uniform_float() - 0.5f;
    return normalize(vec3(x,y,z));
}
