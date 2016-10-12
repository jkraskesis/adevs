/**
 * This calculates an "agent based" solution to dx/dt = -ax.
 */
#include "adevs.h"
#include <cmath>
#include <iostream>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
using namespace std;
using namespace adevs;

const bool print_soln = false;
static int num_agents = 2000000;
static const double a = 1.0;
static gsl_rng *rnd = gsl_rng_alloc(gsl_rng_default);

class Agent:
	public Atomic<int>
{
	public:
		Agent():
			Atomic<int>(),
			ttg(gsl_ran_exponential(rnd,a))
		{
			pop++;
		}
		void delta_int() { ttg = adevs_inf<double>(); }
		void delta_ext(double,const Bag<int>&){}
		void delta_conf(const Bag<int>&){}
		void output_func(Bag<int>&)
		{
			pop--;
		}
		void gc_output(Bag<int>&){}
		double ta() { return ttg; }
		static int getPop() { return pop; }
	private:
		static int pop;
		double ttg;
};

int Agent::pop = 0;

double run()
{
	double max_error = 0.0;
	SimpleDigraph<int> *world = new SimpleDigraph<int>();
	for (int i = 0; i < num_agents; i++)
	{
		world->add(new Agent());
	}
	Simulator<int> *sim = new Simulator<int>(world);
	if (print_soln)
		cout << 0 << " " <<
			((double)(Agent::getPop())/(double)(num_agents)) << " " <<
			exp(-a*0.0) << endl;
	while (sim->nextEventTime() < adevs_inf<double>())
	{
		double t = sim->nextEventTime();
		sim->execNextEvent();
		double asoln = ((double)(Agent::getPop())/(double)(num_agents));
		double tsoln = exp(-a*t);
		double err = asoln-tsoln;
		max_error = ::max(fabs(err),max_error);
		if (print_soln)
			cout << t << " " << asoln << " " << tsoln << " " << err << endl;
	}
	delete sim;
	delete world;
	return max_error;
}

int main()
{
	for (num_agents = 10000; num_agents < 5000000; num_agents += 10000)
	{
		double err = run();
		cout << num_agents << " " << err << endl;
	}
	return 0;
}

