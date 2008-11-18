#include "adevs.h"
#include "Cell.h"
#include <iostream>
using namespace adevs;
using namespace std;

class Listener:
	public EventListener<car_t*>
{
	public:
		Listener()
		{
			omp_init_lock(&lock);
		}
		void outputEvent(Event<car_t*> x, double t)
		{
			omp_set_lock(&lock);
			Cell* model = dynamic_cast<Cell*>(x.model);
			cout << "Car " << x.value->ID << " left cell " 
				<< model->getPos() << " @ t = " << t << endl;
			omp_unset_lock(&lock);
		}
		void stateChange(Atomic<car_t*>* model, double t, void* data)
		{
			omp_set_lock(&lock);
			Cell* cell = dynamic_cast<Cell*>(model);
			assert(t == cell->getTime(data));
			cout << cell->getMsg(data) << endl;
			omp_unset_lock(&lock);
		}
		~Listener()
		{
			omp_destroy_lock(&lock);
		}
	private:
		omp_lock_t lock;
};

int main()
{
	SimpleDigraph<car_t*>* model = new SimpleDigraph<car_t*>();
	vector<Cell*> road;
	for (unsigned i = 0; i < 10; i++)
	{
		car_t* car = NULL;
		if (i == 0 || i == 5)
		{
			car = new car_t;
			car->ID = i;
			car->spd = 1.0;
			if (i == 0) car->spd = 2.0;
		}
		road.push_back(new Cell(i,car));
		model->add(road[i]);
	}
	for (unsigned i = 0; i < 9; i++)
	{
		model->couple(road[i],road[i+1]);
	}
	OptSimulator<car_t*>* sim = new OptSimulator<car_t*>(model,2);
	sim->addEventListener(new Listener());
	sim->execUntil(2.0);
	sim->execUntil(DBL_MAX);
	delete sim;
	delete model;
	return 0;
}
