/**
 * Copyright (c) 2014, James Nutaro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies, 
 * either expressed or implied, of the FreeBSD Project.
 *
 * Bugs, comments, and questions can be sent to nutaro@gmail.com
 */
#ifndef _adevs_fmi_h_
#define _adevs_fmi_h_
#include <algorithm>
#include <cmath>
#include <iostream>
#include <dlfcn.h>
#include <cstdlib>
#include "adevs_hybrid.h"
#include "fmi2Functions.h"
#include "fmi2FunctionTypes.h"
#include "fmi2TypesPlatform.h"

namespace adevs
{

/**
 * Load an FMI wrapped continuous system model for use in a
 * discrete event simulation. The FMI can then be attached
 * to any of the ODE solvers and event detectors in adevs
 * for simulation with the Hybrid class. This FMI loader 
 * does not automatically extract model information from the
 * description XML, and so that information must be provided
 * explicitly by the end-user, but you probably need to know
 * this information regardless if you are using the FMI inside
 * of a larger discrete event simulation.
 *
 */
template <typename X> class FMI:
	public ode_system<X>
{
	public:
		/**
		 * This constructs a wrapper around an FMI. The constructor
		 * must be provided with the number of state variables,
		 * number of event indicators, and the path to the .so file
		 * that contains the FMI functions for this model.
		 */
		FMI(const char* modelname,
			const char* guid,
			int num_state_variables,
			int num_event_indicators,
			const char* shared_lib_name);
		/// Copy the initial state of the model to q
		virtual void init(double* q);
		/// Compute the derivative for state q and put it in dq
		virtual void der_func(const double* q, double* dq);
		/// Compute the state event functions for state q and put them in z
		virtual void state_event_func(const double* q, double* z);
		/// Compute the time event function using state q
		virtual double time_event_func(const double* q);
		/**
		 * This method is invoked immediately following an update of the
		 * continuous state variables and signal to the FMI the end
		 * of an integration state.
		 */
		virtual void postStep(double* q);
		/**
		 * The internal transition function. This function will process all events
		 * required by the FMI. Any derived class should call this method for the
		 * parent class, then set or get any variables as appropriate, and then
		 * call the base class method again to account for these changes. 
		 */
		virtual void internal_event(double* q,
				const bool* state_event);
		/**
		 * The external transition See the notes on the internal_event function for
		 * derived classes.
		 */
		virtual void external_event(double* q, double e,
				const Bag<X>& xb);
		/**
		 * The confluent transition function. See the notes on the internal_event function for
		 * derived classes.
		 */
		virtual void confluent_event(double *q, const bool* state_event,
				const Bag<X>& xb);
		/**
		 * The output function. This can read variables for the FMI, but should
		 * not make any modifications to those variables.
		 */
		virtual void output_func(const double *q, const bool* state_event,
				Bag<X>& yb);
		/**
		 * Garbage collection function. This works just like the Atomic gc_output method.
		 * The default implementation does nothing.
		 */
		virtual void gc_output(Bag<X>& gb);
		/// Destructor
		virtual ~FMI();
		// Get the current time
		double get_time() const { return t_now; }
		// Get the value of a real variable
		double get_real(int k);
		// Get the value of an integer variable
		int get_int(int k);
		// Get the value of a boolean variable
		bool get_bool(int k);

	private:
		// Reference to the FMI
		fmi2Component c;
		// Pointer to the FMI interface
		fmi2Component (*_fmi2Instantiate)(fmi2String, fmi2Type,
				fmi2String, fmi2String, const fmi2CallbackFunctions*,
				fmi2Boolean, fmi2Boolean);
		void (*_fmi2FreeInstance)(fmi2Component);
		fmi2Status (*_fmi2SetupExperiment)(fmi2Component, fmi2Boolean,
				fmi2Real, fmi2Real, fmi2Boolean, fmi2Real);
		fmi2Status (*_fmi2EnterInitializationMode)(fmi2Component);
		fmi2Status (*_fmi2ExitInitializationMode)(fmi2Component);
		fmi2Status (*_fmi2GetReal)(fmi2Component, const fmi2ValueReference*, size_t, fmi2Real*);
		fmi2Status (*_fmi2GetInteger)(fmi2Component, const fmi2ValueReference*, size_t, fmi2Integer*);
		fmi2Status (*_fmi2GetBoolean)(fmi2Component, const fmi2ValueReference*, size_t, fmi2Boolean*);
		fmi2Status (*_fmi2GetString)(fmi2Component, const fmi2ValueReference*, size_t, fmi2String*);
		fmi2Status (*_fmi2SetReal)(fmi2Component, const fmi2ValueReference*, size_t, const fmi2Real*);
		fmi2Status (*_fmi2SetInteger)(fmi2Component, const fmi2ValueReference*, size_t, const fmi2Integer*);
		fmi2Status (*_fmi2SetBoolean)(fmi2Component, const fmi2ValueReference*, size_t, const fmi2Boolean*);
		fmi2Status (*_fmi2SetString)(fmi2Component, const fmi2ValueReference*, size_t, const fmi2String*);
		fmi2Status (*_fmi2EnterEventMode)(fmi2Component);
		fmi2Status (*_fmi2NewDiscreteStates)(fmi2Component,fmi2EventInfo*);
		fmi2Status (*_fmi2EnterContinuousTimeMode)(fmi2Component);
		fmi2Status (*_fmi2CompletedIntegratorStep)(fmi2Component, fmi2Boolean, fmi2Boolean*, fmi2Boolean*);
		fmi2Status (*_fmi2SetTime)(fmi2Component, fmi2Real);
		fmi2Status (*_fmi2SetContinuousStates)(fmi2Component, const fmi2Real*, size_t);
		fmi2Status (*_fmi2GetDerivatives)(fmi2Component, fmi2Real*, size_t);
		fmi2Status (*_fmi2GetEventIndicators)(fmi2Component, fmi2Real*, size_t);
		fmi2Status (*_fmi2GetContinuousStates)(fmi2Component, fmi2Real*, size_t);
		// Callback functions for the FMI
		fmi2CallbackFunctions callbackFuncs;
		// Instant of the next time event
		double next_time_event;
		// Current time
		double t_now;
		// so library handle
		void* so_hndl;
		// Are we in continuous time mode?
		bool cont_time_mode;

		static void fmilogger(
			fmi2ComponentEnvironment componentEnvironment,
			fmi2String instanceName,
			fmi2Status status,
			fmi2String category,
			fmi2String message, ...)
		{
			std::cerr << message << std::endl;
		}
};

template <typename X>
FMI<X>::FMI(const char* modelname,
			const char* guid,
			int num_state_variables,
			int num_event_indicators,
			const char* so_file_name):
	// One extra variable at the end for time
	ode_system<X>(num_state_variables+1,num_event_indicators),
	next_time_event(adevs_inf<double>()),
	t_now(0.0),
	so_hndl(NULL),
	cont_time_mode(false)
{
	so_hndl = dlopen(so_file_name, RTLD_LAZY);
	if (!handle)
	{
		throw adevs::exception("Could not load so file",this);
    }
	_fmi2Instantiate = dlsym(so_hndl,"fmi2Instantiate");
	_fmi2FreeInstance = dlsym(so_hndl,"fmi2FreeInstance");
	_fmi2SetupExperiment = dlsym(so_hndl,"fmi2SetupExperiment");
	_fmi2EnterInitializationMode = dlsym(so_hndl,"fmi2EnterInitializationMode");
	_fmi2ExitInitializationMode= dlsym(so_hndl,"fmi2ExitInitializationMode");
	_fmi2GetReal = dlsym(so_hndl,"fmi2GetReal");
	_fmi2GetInteger = dlsym(so_hndl,"fmi2GetInteger");
	_fmi2GetBoolean = dlsym(so_hndl,"fmi2GetBoolean");
	_fmi2GetString = dlsym(so_hndl,"fmi2GetString");
	_fmi2SetReal = dlsym(so_hndl,"fmi2SetReal");
	_fmi2SetInteger = dlsym(so_hndl,"fmi2SetInteger");
	_fmi2SetBoolean = dlsym(so_hndl,"fmi2SetBoolean");
	_fmi2SetString = dlsym(so_hndl,"fmi2SetString");
	_fmi2EnterEventMode = dlsym(so_hndl,"fmi2EnterEventMode");
	_fmi2NewDiscreteStates = dlsym(so_hndl,"fmi2NewDiscreteStates");
	_fmi2EnterContinuousTimeMode = dlsym(so_hndl,"fmi2EnterContinuousTimeMode");
	_fmi2CompletedIntegratorStep = dlsym(so_hndl,"fmi2CompletedIntegratorStep");
	_fmi2SetTime = dlsym(so_hndl,"fmi2SetTime");
	_fmi2SetContinuousStates = dlsym(so_hndl,"fmi2SetContinuousStates");
	_fmi2GetDerivatives = dlsym(so_hndl,"fmi2GetDerivatives");
	_fmi2GetEventIndicators = dlsym(so_hndl,"fmi2GetEventIndicators");
	_fmi2GetContinuousStates = dysym(so_hndl,"fmi2GetContinuousStates");
	// Create the FMI component
	callbackFuncs.logger = adevs::FMI<X>::fmilogger;
	callbackFuncs.allocateMemory = calloc;
	callbackFuncs.freeMemory = free;
	callbackFuncs.stepFinished = NULL;
	callbackFuncs.componentEnvironment = NULL;
	c = _fmi2Instantiate(modelname,fmi2ModelExchange,guid,"",&callbackFuncs,fmi2False,fmi2False);
	assert(c != NULL);
	_fmi2SetupExperiment(c,fmi2False,-1.0,-1.0,fmi2False,-1.0);
}

template <typename X>
void FMI<X>::init(double* q)
{
	// Set initial value for time
	_fmi2SetTime(c,t_now);
	// Initialize all variables
	_fmi2EnterInitializationMode(c);
	_fmi2ExitInitializationMode(c);
	// Put into consistent initial state
	fmi2EventInfo eventInfo;
	_fmi2NewDiscreteStates(c,&eventInfo);
	if (eventInfo.nextEventTimeDefined == fmi2True)
		next_time_event = eventInfo.nextEventTime;
	_fmi2EnterContinuousTimeMode(c);
	_fmi2GetContinuousStates(c,q,this->numVars()-1);
	q[this->numVars()-1] = t_now;
	cont_time_mode = true;
}

template <typename X>
void FMI<X>::der_func(const double* q, double* dq)
{
	if (!cont_time_mode)
	{
		_fmi2EnterContinuousTimeMode(c);
		cont_time_mode = true;
	}
	_fmi2SetTime(c,q[this->numVars()-1]);
	_fmi2SetContinuousStates(c,q,this->numVars()-1);
	_fmi2GetDerivatives(c,dq,this->numVars()-1);
	dq[this->numVars()-1] = 1.0;
}

template <typename X>
void FMI<X>::state_event_func(const double* q, double* z)
{
	if (!cont_time_mode)
	{
		_fmi2EnterContinuousTimeMode(c);
		cont_time_mode = true;
	}
	_fmi2SetTime(c,q[this->numVars()-1]);
	_fmi2SetContinuousStates(c,q,this->numVars()-1);
	_fmi2GetEventIndicators(c,z,this->numEvents());
}

template <typename X>
double FMI<X>::time_event_func(const double* q)
{
	return next_time_event-q[this->numVars()-1];
}

template <typename X>
void FMI<X>::postStep(double* q)
{
	assert(cont_time_mode);
	fmi2Boolean enterEventMode;
	fmi2Boolean terminateSimulation;
	t_now = q[this->numVars()-1];
	_fmi2SetTime(c,t_now);
	_fmi2SetContinuousStates(c,q,this->numVars()-1);
	_fmi2CompletedIntegratorStep(c,fmi2True,&enterEventMode,&terminateSimulation);
	if (enterEventMode == fmi2True)
		next_time_event = t_now;
}

template <typename X>
void FMI<X>::internal_event(double* q, const bool* state_event)
{
	// postStep will have updated the continuous variables, so 
	// we just process discrete events here.
	_fmi2EnterEventMode(c);
	cont_time_mode = false;
	fmi2EventInfo eventInfo;
	do
	{
		_fmi2NewDiscreteStates(c,&eventInfo);
	}
	while(eventInfo.newDiscreteStatesNeeded == fmi2True);
	if (eventInfo.nextEventTimeDefined == fmi2True)
	{
		next_time_event = eventInfo.nextEventTime;
	}
	else next_time_event = adevs_inf<double>();
	_fmi2GetContinuousStates(c,q,this->numVars()-1);
}
				
template <typename X>
void FMI<X>::external_event(double* q, double e, const Bag<X>& xb)
{
	// Go to event mode if we have not yet done so
	if (cont_time_mode)
	{
		_fmi2EnterEventMode(c);
	}
	// Otherwise, process any events that need processing
	else
	{
		fmi2EventInfo eventInfo;
		do
		{
			_fmi2NewDiscreteStates(c,&eventInfo);
		}
		while(eventInfo.newDiscreteStatesNeeded == fmi2True);
		if (eventInfo.nextEventTimeDefined == fmi2True)
		{
			next_time_event = eventInfo.nextEventTime;
		}
		else next_time_event = adevs_inf<double>();
		_fmi2GetContinuousStates(c,q,this->numVars()-1);
	}
}
				
template <typename X>
void FMI<X>::confluent_event(double *q, const bool* state_event, const Bag<X>& xb)
{
	// postStep will have updated the continuous variables, so 
	// we just process discrete events here.
	_fmi2EnterEventMode(c);
	cont_time_mode = false;
	fmi2EventInfo eventInfo;
	do
	{
		_fmi2NewDiscreteStates(c,&eventInfo);
	}
	while(eventInfo.newDiscreteStatesNeeded == fmi2True);
	if (eventInfo.nextEventTimeDefined == fmi2True)
	{
		next_time_event = eventInfo.nextEventTime;
	}
	else next_time_event = adevs_inf<double>();
	_fmi2GetContinuousStates(c,q,this->numVars()-1);
}
				
template <typename X>
void FMI<X>::output_func(const double *q, const bool* state_event, Bag<X>& yb)
{
}

template <typename X>
void FMI<X>::gc_output(Bag<X>& gb)
{
}

template <typename X>
FMI<X>::~FMI()
{
	_fmi2FreeInstance(c);
	dlclose(so_hndl);
}

template <typename X>
double FMI<X>::get_real(int k)
{
	const fmi2ValueReference ref = k;
	fmi2Real val;
	_fmi2GetReal(c,&ref,1,&val);
	return val;
}

template <typename X>
int FMI<X>::get_int(int k)
{
	const fmi2ValueReference ref = k;
	fmi2Integer val;
	_fmi2GetInteger(c,&ref,1,&val);
	return val;
}

template <typename X>
bool FMI<X>::get_bool(int k)
{
	const fmi2ValueReference ref = k;
	fmi2Boolean val;
	_fmi2GetBoolean(c,&ref,1,&val);
	return (val == fmi2True);
}

} // end of namespace

#endif
