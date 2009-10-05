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
#include "adevs_time.h"

namespace adevs
{

/*
 * This is an empty class definition for compilers that do not support OpenMP.
 */
template <typename X> struct LogicalProcess
{
	void addModel(Devs<X>*){}
	Time getNextEventTime() { return Time::Inf(); } 
	int getID() const { return 0; }
	void run(double) {}
	void outputEvent(Event<X>, double){}
	void stateChange(Atomic<X>*, double){}
	void notifyInput(Atomic<X>*, X&){}
};

} // end of namespace 

