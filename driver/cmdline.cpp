/*
 * 	Command line parsing.
 * 	By the use of -l and -s this is kept to a minimum.
 * 	Cmdline options should superseede the script's options.
 *
 */
#include "cmdline.h"

#include "libgi/rt.h"

#include <argp.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include <sstream>

using namespace std;

const char *argp_program_version = "1";

static char doc[]       = "rt: a simple ray tracer.";
static char args_doc[]  = "";

// long option without corresponding short option have to define a symbolic constant >= 300
enum { FIRST = 300 };

static struct argp_option options[] = 
{
	// --[opt]		short/const		arg-descr		flag	option-descr
	{ "verbose", 			'v', 	0,         		0, "Be verbose." },
	{ "script",             's',    "file",         0, "Use file instead of stdin to read commands from" },
	{ "load",               'l',    "file",         0, "Read commands from file, then from stdin" },
	{ "outfile",            'o',    "file",         0, "Store generated image (in png format) to this file" },
	{ 0 }
};

string& replace_nl(string &s)
{
	for (int i = 0; i < s.length(); ++i)
		if (s[i] == '\n' || s[i] == '\r')
			s[i] = ' ';
	return s;
}

vec3 read_vec3(const std::string &s) {
	istringstream iss(s);
	vec3 v;
	char sep;
	iss >> v.x >> sep >> v.y >> sep >> v.z;
	return v;
}


static error_t parse_options(int key, char *arg, argp_state *state)
{
	// call argp_usage to stop program execution if something is wrong
	string sarg;
	if (arg)
		sarg = arg;
	sarg = replace_nl(sarg);

	switch (key)
	{
	case 's':   cmdline.script = sarg; cmdline.interact = false; break;
	case 'l':   cmdline.script = sarg; break;
	case 'o':   cmdline.outfile = sarg; break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp parser = { options, parse_options, args_doc, doc };

int parse_cmdline(int argc, char **argv)
{
	int ret = argp_parse(&parser, argc, argv, /*ARGP_NO_EXIT*/0, 0, 0);
	return ret;
}
	
struct cmdline cmdline;

