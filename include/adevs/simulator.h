/*
 * Copyright (c) 2013, James Nutaro
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
 * ANY EValueTypePRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EValueTypeEMPLARY, OR CONSEQUENTIAL DAMAGES
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

#ifndef _adevs_simulator_h_
#define _adevs_simulator_h_

#include <cassert>
#include <cstdlib>
#include <list>
#include <set>
#include "adevs/graph.h"
#include "adevs/models.h"
#include "adevs/sched.h"


namespace adevs {

/**
 * @brief An interface for receiving notifications of state changes,
 * input events, and output events from a running simulation.
 * 
 * The EventListener interface is used to be notified of events as
 * they occur in a simulation. It must be registered with the Simulator
 * that will provide the notifications.
 */
template <typename ValueType, typename TimeType = double>
class EventListener {
  public:
    /**
     * @brief The virtual default constructor.
     * 
     * The default constructor is empty and does not perform any
     * initialization.
     */
    virtual ~EventListener() {}
    /**
     * @brief Called when a Atomic model produces an output.
     * 
     * This method is called for each PinValue appearing in the
     * list of outputs produced by the Atomic model's output_func()
     * methods, including the MealyAtomic forms of output_func().
     * 
     * @param model The Atomic model that produced the output
     * @param value The PinValue created by the model
     * @param t The time of the output event
     */
    virtual void outputEvent(Atomic<ValueType, TimeType> &model,
                             PinValue<ValueType> &value, TimeType t) = 0;
    /**
     * @brief Called when an Atomic receives an input.
     * 
     * This method is called for each PinValue that is passed to the
     * Atomic model's delta_ext() and delta_conf() methods.
     * 
     * @param model The Atomic model that receives the input
     * @param value The PinValue provided as input
     * @param t The time of the input event
     */
    virtual void inputEvent(Atomic<ValueType, TimeType> &model,
                            PinValue<ValueType> &value, TimeType t) = 0;
    /**
     * @brief Called after an Atomic model changes its state.
     * 
     * This method is called after the Atomic model's delta_int(),
     * delta_ext(), and delta_conf() methods are called.
     * 
     * @param model The Atomic model that changed state
     * @param t The time when the change occurred
     */
    virtual void stateChange(Atomic<ValueType, TimeType> &model,
                             TimeType t) = 0;
};

/**
 * @brief Implements the DEVS simulation algorithm.
 *  
 * This Simulator class implements the DEVS simulation algorithm.
 * Its methods throw adevs::exception objects if any of the DEVS model
 * constraints are violated (e.g., a negative time advance). The Simulation
 * algorithm can be described briefly as follows:
 * 1. Each Atomic model has an associated clock called its elapsed time, denoted by e.
 * This value is initialized to zero. The current simulation time is set to zero.
 * 2. Call the ta() method of each Atomic model and find the smallest value of ta()-e.
 * The set of models whose ta()-e equals this smallest value are the imminent models.
 * If the smallest ta()-e is infinity, then the simulation end. For brevity, we use
 * dt to indicate this smallest ta()-e.
 * 3. Add dt to the elapsed time e of each Atomic model and to the current simulation
 * time. Notice that imminent models now have e = ta() and non-imminent models
 * have e < ta().
 * 4. Call the output_func() method of each imminent model that is not a MealyAtomic
 * and collect these output values in a list. The imminent models that are not MealyAtomic
 * models are put into the active set. The imminent models that are MealyAtomic are put
 * into the pending set. The pending set contains only MealyAtomic models. Now
 * do the following:
 *     -# For each PinValue pair in the output list, use the Graph route() method to find
 * the models that receive this value as input. The receiving models that are not
 * MealyAtomic go into the active set. The MealyAtomic receivers go into the pending set.
 * If a MealyAtomic model already in the active set is added to the pending set,
 * then an exception is thrown and the simulation is aborted.
 *     -# If the pending set is empty, then go to step 5.
 *     -# Select a MealyAtomic model from the pending set and move it to the active set.
 *     -# Call the selected model's output function to get a new output list
 *         - If the model has received no input and is imminent, call output_func()
 *         - If the model has received input and is imminent, call confluent_output_func()
 *         - If the model has received input and is not imminent, call external_output_func()
*      -# Go to step 4.a. 
 * 5. Calculate new states for the Atomic models.
 *     - If the model is imminent and is not in the active set, call its delta_int() method
 * and set its elapsed time e to zero. 
 *     - If the model is imminent and is in the active set, call its delta_conf() method
 * with the input produced in step 4. Set its elapsed time e to zero.         
 *     - If the model is not imminent and is in the active set, call its delta_ext() method
 * with its elapsed time e and the input produced in step 4. Set its elapsed time e to zero.
 *     - Otherwise, do nothing.
 * 6. Apply provisional changes to the Graph structure.
 * 7. Go to step 2.
 * 
 * Looking at this algorithm we can see two important features. First, the time advance ta()
 * of each model is used to determine the time of the next event. The value returned by time advance 
 * is how long the Atomic model will stay in its current state before changing state spontaneously.
 * Second, the current simulation time is the sum of the dt values calculated in each iteration
 * of the algorithm.
 * 
 * The Simulator is designed to be used as a component within a larger simulation.
 * To illustrate how the Simulator can be used in this way, let us sketch its use to
 * create a federate as described in the IEEE 1516 (High Level Architecture) standard.
 * This procedure assumes you have no MealyAtomic models that share data with the
 * federation. The general idea is as follows:
 * 1. Register one or more EventListener objects that will gather output events that are to
 * be published to the HLA federation.
 * 2. Get your time of next event by calling the Simulator nextEventTime() method.
 * 3. Call computeNextOutput(). Send Events and Object Updates (i.e., HLA messages)
 * as your registered EventListener objects receive output events. The time stamp for
 * these messages is the time returned by nextEventTime(). Make sure they are published
 * as retractable messages so that they can be canceled if you receive input before
 * your next event time.
 * 4. Issue a next event request to the HLA federation for your nextEventTime().
 * 5. You will receive one or more messages from the federation, each with an identical
 * time stamp.
 *   -# If this time stamp is less than your nextEventTime() then call setNextTime()
 * with the time stamp of the received message and retract any messages published in
 * step 3.
 *   -# Inject the data from the received messages into your Simulator by using the injectInput() method.
 * 6. You will receive a time advance grant from the federation.
 *   -# If the grant is equal to the time you requested, call computeNextState().
 *   -# If the grant is less than the time you requested, call execNextEvent(). MealyAtomic
 * models may produce output at this time, but non MealyAtomic models will not.
 * 7. Repeat from step 2.
 */
template <class ValueType, class TimeType = double>
class Simulator {

  public:
    /** 
     * @brief Create a simulator for the atomic model.
     * 
     * The constructor will fail and throw an adevs::exception if the
     * time advance of the model is less than zero.
     * @param model The model to simulate.
     */
    Simulator(shared_ptr<Atomic<ValueType, TimeType>> model);

    /**
     * @brief Initialize the simulator with a collection of models.
     
     * The constructor will fail and throw an adevs::exception if the
     * time advance of the model is less than zero.
     * @param model The graph to simulate.
     */
    Simulator(std::shared_ptr<Graph<ValueType, TimeType>> model);

    /**
     * @brief Initialize the simulator with a Coupled model.
     * 
     * @param model The Coupled model to simulate.
     */
    Simulator(std::shared_ptr<Coupled<ValueType, TimeType>> model);

    /**
     * @brief Get the time of the next event.
     
     * This is the absolute time of the next output and change of state.
     * 
     * @return The absolute time of the next event.
     */
    TimeType nextEventTime() { return tNext; }

    /** 
     * @brief Execute the simulation cycle at the next event time.
     * 
     * Update the simulation time to match nextEventTime() and calculate
     * the output and next states as described in steps 4, 5, and 6 of the
     * simulation algorithm.
     * 
     * @return The current simulation time
     */
    TimeType execNextEvent() {
        computeNextOutput();
        return computeNextState();
    }

    /**
     * @brief Inject an event into the simulation.
     * 
     * Inject an input that will be applied at the next call to
     * computeNextOutput(). The event is routed to each model that
     * is reachable from the pin of the injected PinValue object.
     * 
     * @param x The PinValue to inject into the simulation.
     */
    void injectInput(PinValue<ValueType> &x) {
        external_input.push_back(x);
        // We we need to recompute Mealy model outputs
    }

    /**
     * @brief Clear the list of injected inputs.
     * 
     * This erases all injected inputs that have not yet been applied
     * to the simulation via a call to computeNextOutput().
     * It is cleared automatically at each call to computeNextOutput(),
     */
    void clearInjectedInput() {
        external_input.clear();
    }

    /**
     * @brief Set the next event time to something less than the
     * nextEventTime() method would return.
     * 
     * This is used to force the simulator to apply injected inputs at 
     * the supplied time.
     * 
     * @param t The time at which the next event will occur.
     */
    void setNextTime(TimeType t) {
        tNext = t;
    }

    /**
     * @brief Compute the output values of models at the next event time.
     * 
     * Output is produced by models imminent at the next event time, 
     * MealyAtomic models that receive input from other models, and
     * MealyAtomic models that receive input injected into the simulation.
     * This method notifies EventListener as output is produced. It
     * does not change the simulation time or states of the models.
     */
    void computeNextOutput();

    /**
     * @brief Compute the next state of the model.
     * 
     * This notifies EventListener as inputs are applied to models
     * and as new states are calculated. Provisional changes to
     * the model structure are applied after new states are computed
     * for the Atomic components.
     * 
     * @return The current simulation time.
     */
    TimeType computeNextState();

    /**
     * @brief Register an EventListener with the Simulator.
     * 
     * Add an event listener to the simulator that will be notified of
     * input, output, and changes of state as they occur.
     * 
     * @param listener The EventListener to be added.
     */
    void addEventListener(
        std::shared_ptr<EventListener<ValueType, TimeType>> listener) {
        listeners.push_back(listener);
    }

  private:
    std::shared_ptr<Graph<ValueType, TimeType>> graph;
    std::list<shared_ptr<EventListener<ValueType, TimeType>>> listeners;
    std::list<PinValue<ValueType>> external_input;
    std::set<Atomic<ValueType, TimeType>*> active;
    Schedule<ValueType, TimeType> sched;
    TimeType tNext;

    void schedule(Atomic<ValueType, TimeType>* model, TimeType t);
};

template <typename ValueType, typename TimeType>
Simulator<ValueType, TimeType>::Simulator(
    std::shared_ptr<Graph<ValueType, TimeType>> model)
    : graph(model) {
    graph->set_provisional(true);
    for (auto atomic : model->get_atomics()) {
        schedule(atomic.get(), adevs_zero<TimeType>());
    }
    tNext = sched.minPriority();
}

template <typename ValueType, typename TimeType>
Simulator<ValueType, TimeType>::Simulator(
    std::shared_ptr<Atomic<ValueType, TimeType>> model)
    : graph(new Graph<ValueType, TimeType>()) {
    graph->add_atomic(model);
    graph->set_provisional(true);
    schedule(model.get(), adevs_zero<TimeType>());
    tNext = sched.minPriority();
}

template <typename ValueType, typename TimeType>
Simulator<ValueType, TimeType>::Simulator(
    std::shared_ptr<Coupled<ValueType, TimeType>> model)
    : graph(new Graph<ValueType, TimeType>()) {
    model->assign_to_graph(graph.get());
    graph->set_provisional(true);
    for (auto atomic : graph->get_atomics()) {
        schedule(atomic.get(), adevs_zero<TimeType>());
    }
    tNext = sched.minPriority();
}

template <class ValueType, class TimeType>
void Simulator<ValueType, TimeType>::computeNextOutput() {
    PinValue<ValueType> x;
    std::list<std::pair<pin_t, std::shared_ptr<Atomic<ValueType, TimeType>>>> input;
    std::set<MealyAtomic<ValueType, TimeType>*> pending_active;
    // Undo prior output calculation
    for (auto model : active) {
        model->outputs.clear();
        model->inputs.clear();
    }
    active.clear();
    // Route externally supplied inputs
    for (auto y : external_input) {
        x.value = y.value;
        graph->route(y.pin, input);
        for (auto consumer : input) {
            if (consumer.second->isMealyAtomic() != nullptr) {
                // Mealy models outputs are calculated after Moore models
                // because the Mealy output may depend on the Moore output
                pending_active.insert(consumer.second->isMealyAtomic());
            } else {
                active.insert(consumer.second.get());
            }
            x.pin = consumer.first;
            consumer.second->inputs.push_back(x);
        }
        input.clear();
    }
    external_input.clear();
    // Route the output from the Moore type imminent models
    if (sched.minPriority() == tNext) {
        std::list<Atomic<ValueType, TimeType>*> imm(sched.visitImminent());
        for (auto model : imm) {
            if (model->isMealyAtomic() != nullptr) {
                // Wait to calculate Mealy outputs until we have the
                // output from all of the Moore models
                pending_active.insert(model->isMealyAtomic());
                continue;
            } else {
                active.insert(model);
            }
            model->output_func(model->outputs);
            for (auto y : model->outputs) {
                for (auto listener : listeners) {
                    listener->outputEvent(*model, y, tNext);
                }
                x.value = y.value;
                graph->route(y.pin, input);
                for (auto consumer : input) {
                    if (consumer.second->isMealyAtomic() != nullptr) {
                        // Wait to calculate Mealy outputs until we have the
                        // output from all of the Moore models
                        pending_active.insert(consumer.second->isMealyAtomic());
                    } else {    
                        active.insert(consumer.second.get());
                    }
                    x.pin = consumer.first;
                    consumer.second->inputs.push_back(x);
                }
                input.clear();
            }
        }
    }
    // Calculate output from Mealy type models
    while (!pending_active.empty()) {
        auto model = *(pending_active.begin());
        pending_active.erase(model);
        // This Mealy model must not receive input once its
        // output is calculated. The fact that we have calculated
        // its output is signified by putting it into the active set
        active.insert(model);
        if (model->inputs.empty() && model->tN == tNext) {
            // Internal event
            model->output_func(model->outputs);
        } else if (model->tN == tNext) {
            // Confluent event
            model->confluent_output_func(model->inputs,model->outputs);
        } else {
            // External event
            model->external_output_func(tNext - model->tL, model->inputs, model->outputs);
        }
        for (auto y : model->outputs) {
            for (auto listener : listeners) {
                listener->outputEvent(*model, y, tNext);
            }
            x.value = y.value;
            graph->route(y.pin, input);
            for (auto consumer : input) {
                if (consumer.second->isMealyAtomic() != nullptr) {
                    // If this Mealy model is already active then we have
                    // a feedback loop of Mealy models which is illegal
                    if (active.find(consumer.second.get()) != active.end()) {
                        throw adevs::exception("Feedback loop of Mealy models is illegal", model);
                    }
                    pending_active.insert(consumer.second->isMealyAtomic());
                } else {    
                    active.insert(consumer.second.get());
                }
                x.pin = consumer.first;
                consumer.second->inputs.push_back(x);
            }
            input.clear();
        }
    }
}

template <class ValueType, class TimeType>
TimeType Simulator<ValueType, TimeType>::computeNextState() {
    PinValue<ValueType> x;
    TimeType t = tNext + adevs_epsilon<TimeType>();
    for (auto model : active) {
        // Notify listeners of input events
        for (auto x : model->inputs) {
            for (auto listener : listeners) {
                listener->inputEvent(*model, x, tNext);
            }
        }
        // Internal event if no input
        if (model->inputs.empty()) {
            model->delta_int();
        } else if (model->tN == tNext) {
            // Confluent event if model is imminent and has input
            model->delta_conf(model->inputs);
            model->inputs.clear();
        } else {
            // External event if model is not imminent and has input
            model->delta_ext(tNext - model->tL, model->inputs);
            model->inputs.clear();
        }
        for (auto listener : listeners) {
            listener->stateChange(*model, tNext);
        }
        model->outputs.clear();
        // Adjust position in the schedule
        schedule(model, t);
    }
    active.clear();
    // Effect any changes in the model structure
    graph->set_provisional(false);
    std::list<typename Graph<ValueType,TimeType>::graph_op>& pending = graph->get_pending();
    while (!pending.empty()) {
        auto op = pending.front();
        pending.pop_front();
        switch(op.op)
        {
            case Graph<ValueType, TimeType>::ADD_ATOMIC:
                graph->add_atomic(op.model);
                schedule(op.model.get(), t);
                break;
            case Graph<ValueType, TimeType>::REMOVE_ATOMIC:
                sched.schedule(op.model.get(), adevs_inf<TimeType>());
                graph->remove_atomic(op.model);
                break;
            case Graph<ValueType, TimeType>::REMOVE_PIN:
                graph->remove_pin(op.pin[0]);
                break;
            case Graph<ValueType, TimeType>::CONNECT_PIN_TO_PIN:
                graph->connect(op.pin[0], op.pin[1]);
                break;
            case Graph<ValueType, TimeType>::DISCONNECT_PIN_FROM_PIN:
                graph->disconnect(op.pin[0], op.pin[1]);
                break;
            case Graph<ValueType, TimeType>::CONNECT_PIN_TO_ATOMIC:
                graph->connect(op.pin[0], op.model);
                break;
            case Graph<ValueType, TimeType>::DISCONNECT_PIN_FROM_ATOMIC:
                graph->disconnect(op.pin[0], op.model);
                break;
        }
    }
    graph->set_provisional(true);
    // Get the time of next event and return
    tNext = sched.minPriority();
    return t;
}

template <class ValueType, class TimeType>
void Simulator<ValueType, TimeType>::schedule(
    Atomic<ValueType, TimeType>* model, TimeType t) {
    model->tL = t;
    TimeType dt = model->ta();
    if (dt == adevs_inf<TimeType>()) {
        model->tN = adevs_inf<TimeType>();
        sched.schedule(model, adevs_inf<TimeType>());
    } else {
        model->tN = model->tL + dt;
        if (dt < adevs_zero<TimeType>()) {
            exception err("Negative time advance", model);
            throw err;
        }
        sched.schedule(model, model->tN);
    }
}

}  // namespace adevs

#endif
