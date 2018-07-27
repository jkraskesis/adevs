#include "adevs.h"
#include <iostream>
using namespace std;
using namespace adevs;

static const double period = 0.001;

class Genr:
	public Atomic<int>
{
	public:
		Genr():Atomic<int>(){}
		double ta() { return period; }
		void delta_int(){}
		void delta_ext(double, const Bag<int>&){}
		void delta_conf(const Bag<int>&){}
		void output_func(Bag<int>& yb) { yb.insert(1); }
		void gc_output(Bag<int>&){}
};

class test_model:
	public ode_system<int>
{
	public:
		test_model():
		ode_system<int>(1,0)
		{
		}
		void init(double* q)
		{
			q[0] = period;
		}
		void der_func(const double* q, double* dq)
		{
			dq[0] = -1.0;
		}
		void state_event_func(const double* q, double* z){}
		double time_event_func(const double* q)
		{
			return q[0];
		}
		void internal_event(double* q, const bool* event_flag)
		{
			cout << "internal: " << q[0] << endl;
			q[0] = period;
		}
		void external_event(double* q, double e, const Bag<int>& xb)
		{
			cout << "external: " << q[0] << " " << e << " " << xb.size() << endl;
		}
		void confluent_event(double* q, const bool* event_flag, const Bag<int>& xb)
		{
			cout << "confluent" << endl;
			internal_event(q,event_flag);
			external_event(q,0.0,xb);
			cout << "end confluent" << endl;
		}
		void output_func(const double*, const bool*, Bag<int>&){}
		void gc_output(Bag<int>&){}
};

void run_test(ode_system<int>* b,
	ode_solver<int> *s,
	event_locator<int>* l)
{
	Hybrid<int>* ball =
		new Hybrid<int>(b,s,l);
	Genr* genr = new Genr();
	SimpleDigraph<int>* model = new SimpleDigraph<int>();
	model->add(ball);
	model->add(genr);
	model->couple(genr,ball);
	Simulator<int>* sim = new Simulator<int>(model);
	while (sim->nextEventTime() < 10.0)
	{
		sim->execNextEvent();
	}
	delete sim;
	delete model;
}

int main()
{
	// Test linear algorithm
	test_model* ball = new test_model();
	run_test(ball,new corrected_euler<int>(ball,1E-6,0.01),
			new linear_event_locator<int>(ball,1E-7)); 
	return 0;
}
