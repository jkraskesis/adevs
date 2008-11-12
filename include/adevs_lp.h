/***************
Copyright (C) 2008 by James Nutaro

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Bugs, comments, and questions can be sent to nutaro@gmail.com
***************/
#ifndef __adevs_lp_h_
#define __adevs_lp_h_
#include "adevs.h"
#include "adevs_time.h"
#include <list>
#include <typeinfo>
#include <omp.h>

/**
 * This is an implementation of the time warp simulation algorithm described in
 * J. Nutaro, "On Constructing Optimistic Simulation Algorithms for the Discrete
 * Event System Specification", Transactions on Modeling and Computer Simulation,
 * Vol. V, No. N, pgs xx--yy, 2008.
 *
 * This file contains all of the support classes for the optimistic simulator
 * in adevs_opt_sim.h
 */
namespace adevs
{

// Enumeration of simulation message types
typedef enum
{
	RB, // rollback message
	IO // output (input) message
} MessageType;

// A simulation message
template <class X> struct Message
{
	// The message timestamp
	Time t;
	// The logical process that generated the message
	LogicalProcess<X>* src;
	// The value of the message
	X value;
	// The type of message
	MessageType type;
};

// A state checkpoint
struct CheckPoint
{
	// Time stamp
	Time t;
	// Pointer to the saved state
	void* data;
};

/*
 * A logical process is assigned to every atomic model and it simulates
 * that model optimistically. The atomic model must support state saving
 * and state restoration.
 */
template <class X> class LogicalProcess
{
	public:
		/**
		 * The constructor assigns an atomic model to the logical process.
		 * The active list is a shared list that is used to keep track of
		 * which lps are activated by message delivery in each simulation
		 * round.
		 */
		LogicalProcess(Atomic<X>* model, std::vector<LogicalProcess*>* active_list);
		/** 
		 * Optimistically execute the output function
		 */
		void execOutput();
		/**
		 * Optimistically execute the state transition function
		 */
		void execDeltfunc();
		/**
		 * Do fossil collection.
		 */
		void fossilCollect(Time gvt);
		/**
		 * Get the list of messages that output messages that are thought
		 * to be good. The messages with timestamps less than or equal to
		 * gvt are guaranteed to be correct.
		 */
		std::list<Message<X> >* getOutput() { return &output; }
		/**
		 * Get the list of save stats that are thought
		 * to be good. The states with timestamps less than or equal to
		 * gvt are guaranteed to be correct.
		 */
		std::list<CheckPoint>* getStates() { return &chk_pt; }
		/**
		 * Get the model assigned to this lp.
		 */
		Atomic<X>* getModel() { return model; }
		/**
		 * Send a message to the logical process. This will put the message into the
		 * back of the input queue.
		 */
		void sendMessage(Message<X>& m);
		/**
		 * Get the smallest of the local time of next event and first input message
		 */
		Time getNextEventTime() const
		{
			Time result(DBL_MAX,INT_MAX);
			if (time_advance < DBL_MAX)
				result = tL + Time(time_advance,0);
			if (!avail.empty() && avail.front().t < result)
				result = avail.front().t;
			if (!input.empty() && tMinInput < result)
				result = tMinInput;
			if (rb_pending && rb_time < result)
				result = rb_time;
			return result;
		}
		/**
		 * Get the event time for the current system state.
		 */
		Time getLocalStateTime() const { return tL; }
		/**
		 * Set or clear the active flag.
		 */
		void setActive(bool flag) { model->active = flag; }
		/**
		 * Has this lp been activated in this round.
		 */
		bool isActive() const { return model->active; }
		/**
		 * Get the time of the last commit.
		 */
		Time getLastCommit() const { return lastCommit; }
		/**
		 * Set the time of the last commit.
		 */
		void setLastCommit(Time t) { lastCommit = t; }
		/**
		 * Destructor leaves the atomic model intact.
		 */
		~LogicalProcess();
	private:
		// Time of the last committed state
		Time lastCommit;
		// Time advance in the present state
		double time_advance;
		// The time of the last last event
		Time tL;
		// List of input messages
		std::list<Message<X> > input;
		// lock variable
		omp_lock_t lock;
		/**
		 * All of these lists are sorted by time stamp
		 * with the earliest time stamp at the front.
		 */
		// Time ordered list of messages that are available for processing
		std::list<Message<X> > avail;
		// Time ordered list of messages that have been processed
		std::list<Message<X> > used;
		// Time ordered list of good output messages
		std::list<Message<X> > output;
		// Time ordered list of discarded output messages
		std::list<Message<X> > discard;
		// Time ordered list of checkpoints
		std::list<CheckPoint> chk_pt;
		// Set of lps that I have sent a message to
		std::set<LogicalProcess<X>*> recipients;
		// Is there a pending rollback and what is its timestamp
		bool rb_pending;
		Time rb_time;
		// Smallest input timestamp
		Time tMinInput;
		// Temporary pointer to an active list that is provided by the simulator
		std::vector<LogicalProcess<X>*>* active_list;
		// The atomic model assigned to this logical process
		Atomic<X>* model;
		// Input and output bag for the model. Always clear this before using it.
		Bag<X> io_bag;
		// Insert a message into a timestamp ordered list
		void insert_message(std::list<Message<X> >& l, Message<X>& msg);
		// Route events using the Network models' route methods
		void route(Network<X>* parent, Devs<X>* src, X& x);
};

template <class X>
LogicalProcess<X>::LogicalProcess(Atomic<X>* model, std::vector<LogicalProcess<X>*>* active_list):
	rb_pending(false),
	rb_time(DBL_MAX,INT_MAX),
	active_list(active_list),
	model(model)
{
	// Set the local time of next event
	time_advance = model->ta();
	// The model is intially inactive
	model->active = false;
	// initialize lock
	omp_init_lock(&lock);	
}

template <class X>
void LogicalProcess<X>::fossilCollect(Time gvt)
{
	// Delete old states, but keep one that is less than gvt
	std::list<CheckPoint>::iterator citer = chk_pt.begin(), cnext;
	while (citer != chk_pt.end())
	{
		cnext = citer;
		cnext++;
		if (cnext == chk_pt.end()) break;
		else if ((*cnext).t < gvt)
		{
			model->gc_state((*citer).data);
			citer = chk_pt.erase(citer);
		}
		else citer = cnext;
	}
	// Delete old used messages
	while (!used.empty() && used.front().t < gvt)
	{
		used.pop_front();
	}
	// Delete old output
	io_bag.clear();
	while (!discard.empty() && discard.front().t < gvt) 	
	{
		io_bag.insert(discard.front().value);
		discard.pop_front();
	}
	while (!output.empty() && output.front().t < gvt) 
	{
		io_bag.insert(output.front().value);
		output.pop_front();
	}
	if (!io_bag.empty()) model->gc_output(io_bag);
}

template <class X>
void LogicalProcess<X>::execOutput()
{
	// Setup the message
	Message<X> msg;
	msg.src = this;
	// Send the pending rollback
	if (rb_pending)
	{
		// Create the rollback message
		msg.t = rb_time;
		msg.type = RB;
		// Send it to all of the lp's that we've sent a message to
		typename std::set<LogicalProcess<X>*>::iterator lp_iter;
		lp_iter = recipients.begin(); 
		for (; lp_iter != recipients.end(); lp_iter++)
			(*lp_iter)->sendMessage(msg);
		// Cancel the pending rollback
		rb_pending = false;
		rb_time = Time(DBL_MAX,INT_MAX);
	}
	// Compute and send our next output assuming that
	// an internal event will happen next
	if (time_advance < DBL_MAX)
	{
		// Create the message
		msg.t = tL + Time(time_advance,0);
		msg.type = IO;
		// Compute the model's output
		io_bag.clear();
		model->output_func(io_bag);
		// Send the the output values
		for (typename Bag<X>::iterator iter = io_bag.begin(); 
			iter != io_bag.end(); iter++)
		{
			assert(output.empty() || output.back().t <= msg.t);
			msg.value = *iter;
			output.push_back(msg);
			route(model->getParent(),model,*iter);
		}
	}
}

template <class X>
void LogicalProcess<X>::execDeltfunc()
{
	// Process the input messages
	while (!input.empty())
	{
		// Was a used message actual canceled?
		bool used_msg_cancelled = false;
		// Get the message at the front of the list
		Message<X> msg = input.front();
		input.pop_front();
		// If this is a rollback message then discard later messages from the sender
		if (msg.type == RB)
		{
			// Discard unprocessed messages
			typename std::list<Message<X> >::iterator msg_iter = avail.begin();
			while (msg_iter != avail.end())
			{
				if ((*msg_iter).src == msg.src && (*msg_iter).t >= msg.t)
					msg_iter = avail.erase(msg_iter);
				else 
					msg_iter++;
			}
			// Discard processed messages
			msg_iter = used.begin();
			while (msg_iter != used.end())
			{
				if ((*msg_iter).src == msg.src && (*msg_iter).t >= msg.t)
				{
					msg_iter = used.erase(msg_iter);
					used_msg_cancelled = true;
				}
				else
					msg_iter++;
			}
		}
		// Otherwise add it to the list of available messages
		else
		{
			insert_message(avail,msg);
		}
		// If this message is in the past, then perform a rollback
		if ((msg.type != RB && msg.t < tL) || used_msg_cancelled)
		{
			assert(!chk_pt.empty());
			// Discard the incorrect outputs
			while (!output.empty() && output.back().t > msg.t)
			{
				insert_message(discard,output.back());
				output.pop_back();
			} 
			// Discard incorrect checkpoints
			while (chk_pt.back().t > msg.t)
			{
				model->gc_state(chk_pt.back().data);
				chk_pt.pop_back();
				assert(chk_pt.empty() == false); 
			}
			// Restore the model state 
			tL = chk_pt.back().t;
			model->tL = tL.t; 
			model->restore_state(chk_pt.back().data);
			time_advance = model->ta();
			// Remove the checkpoint
			model->gc_state(chk_pt.back().data);
			chk_pt.pop_back();
			// Make sure the time advance is ok
			if (time_advance < 0.0)
			{
				exception err("Atomic model has a negative time advance",model);
				throw err;
			}
			// Copy used messages back to the available list
			while (!used.empty() && used.back().t >= tL)
			{
				assert(avail.empty() || used.back().t <= avail.front().t);
				avail.push_front(used.back());
				used.pop_back();
			}
			// Schedule a rollback message
			Time t_bad = msg.t + Time(0.0,1);
			if (!rb_pending || (t_bad < rb_time)) rb_time = t_bad;
			rb_pending = true;
		}
	}
	// This is the time of the next internal event
	Time tSelf(DBL_MAX,INT_MAX);
	if (time_advance < DBL_MAX)
		tSelf = tL + Time(time_advance,0);
	Time tN(tSelf);
	io_bag.clear();
	// Look for input
	if (!avail.empty())
	{
		if (avail.front().t < tN)
		{
			tN = avail.front().t; 
		}
		while (!avail.empty() && avail.front().t == tN)
		{
			io_bag.insert(avail.front().value);
			assert(used.empty() || avail.front().t >= used.back().t);
			used.push_back(avail.front());
			avail.pop_front();
		}
	}
	// Did we produce a bad output that must be retracted?
	assert(tN <= tSelf);
	if (!rb_pending && time_advance < DBL_MAX && tN < tSelf)
	{
		rb_pending = true;
		rb_time = tSelf;
		insert_message(discard,output.back());
		output.pop_back();
	}
	// If the next event is at infinite, then there is nothing to do
	if (tN.t == DBL_MAX) return;
	assert(tL <= tN);
	// Save the current state
	CheckPoint c;
	c.t = tL;
	c.data = model->save_state();
	chk_pt.push_back(c);
	// Compute the next state
	if (io_bag.empty())
	{
		model->delta_int();
	}
	else if (tN == tSelf)
	{
		model->delta_conf(io_bag);
	}
	else
	{
		model->delta_ext(tN.t-tL.t,io_bag);
	}
	// Get the new value of the time advance
	time_advance = model->ta();
	// Actual time for this state
	tL = tN + Time(0.0,1);
	model->tL = tL.t;
}

template <class X>
void LogicalProcess<X>::route(Network<X>* parent, Devs<X>* src, X& x)
{
	// No one to do the routing, so return
	if (parent == NULL) return;
	// Create a bag to collect the receivers
	Bag<Event<X> > recvs;
	// Compute the set of receivers for this value
	parent->route(x,src,recvs);
	// Deliver the event to each of its targets
	Atomic<X>* amodel = NULL;
	typename Bag<Event<X> >::iterator recv_iter = recvs.begin();
	for (; recv_iter != recvs.end(); recv_iter++)
	{
		// Check for self-influencing error condition
		if (src == (*recv_iter).model)
		{
			exception err("Model tried to influence self",src);
			throw err;
		}
		/**
		if the destination is an atomic model
		*/
		amodel = (*recv_iter).model->typeIsAtomic();
		if (amodel != NULL)
		{
			Message<X> m = output.back();
			m.value = (*recv_iter).value;
			amodel->lp->sendMessage(m); 
			recipients.insert(amodel->lp); 
		}
		// if this is an external output from the parent model
		else if ((*recv_iter).model == parent)
		{
			route(parent->getParent(),parent,(*recv_iter).value);
		}
		// otherwise it is an input to a coupled model
		else
		{
			route((*recv_iter).model->typeIsNetwork(),
			(*recv_iter).model,(*recv_iter).value);
		}
	}
}

template <class X>
void LogicalProcess<X>::sendMessage(Message<X>& msg)
{
	bool put_in_active_list = false;
	// Lock the input list
	omp_set_lock(&lock);
	// Set the minimum input time
	if (input.empty()) tMinInput = msg.t;
	else if (msg.t < tMinInput) tMinInput = msg.t;
	// Insert the message
	input.push_back(msg);
	// Check the active status
	if (!(model->active))
	{
		put_in_active_list = model->active = true;
	}
	omp_unset_lock(&lock);
	// Insert the message into the global active list
	if (put_in_active_list)
	{
		#pragma omp critical
		{
			active_list->push_back(this);
		}
	} // end of critical section
}

template <class X>
void LogicalProcess<X>::insert_message(std::list<Message<X> >& l, Message<X>& msg)
{
	typename std::list<Message<X> >::iterator msg_iter;
	for (msg_iter = l.begin(); msg_iter != l.end(); msg_iter++)
	{
		if ((*msg_iter).t > msg.t) break;
	}
	l.insert(msg_iter,msg);
}

template <class X>
LogicalProcess<X>::~LogicalProcess()
{
	// Delete check points
	while (!chk_pt.empty())
	{
		model->gc_state(chk_pt.front().data);
		chk_pt.pop_front();
	}
	// Clean up remaining output messages
	io_bag.clear();
	while (!output.empty())
	{
		io_bag.insert(output.front().value);
		output.pop_front();
	}
	// Clean up the discarded messages
	while (!discard.empty())
	{
		io_bag.insert(discard.front().value);
		discard.pop_front();
	}
	if (!io_bag.empty()) model->gc_output(io_bag);
	// destroy lock
	omp_destroy_lock(&lock);
}

} // end of namespace 

#endif
