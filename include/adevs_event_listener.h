/***************
Copyright (C) 2000-2008 by James Nutaro

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
#ifndef __adevs_event_listener_h_
#define __adevs_event_listener_h_
#include "adevs_models.h"
#include "adevs_bag.h"

namespace adevs
{

/**
 * The EventListener class is used to receive output events produced
 * by models, injects injected into models, and to be notified of state changes by
 * Atomic models.
 */
template <class X> class EventListener
{
	public:
		/**
		 * This callback is invoked when a model, network or atomic,
		 * produces an output. The default implementation is empty.
		 */
		virtual void outputEvent(Event<X> x, double t){}
		/**
		 * This callback is made by the simulator after an Atomic
		 * model changes its state. This method has an empty default
		 * implementation.
		 */
		virtual void stateChange(Atomic<X>* model, double t){}
		virtual ~EventListener(){}
};

} // end of namespace

#endif
