/**
 * This file contains definitions of external variable types
 * and other parts of the Open Modelica libsim that cannot
 * be linked to by an adevs module. The libsim runtime seems
 * to take the role of the main program block, with main, the
 * dassl code, its links to dassl specific functions, etc.
 * that are not present in the adevs simulation runtime.
 * Whatever is needed from the library for the adevs modules
 * produced by the modelica compiler is therefore put here.
 *
 */
#include "adevs_modelica_runtime.h"
#include <iostream>
#include <cstdlib>
using namespace std;

int modelErrorCode;

void MODELICA_TERMINATE(const char* msg)
{
	cerr << msg << endl;
	exit(-2);
}

void MODELICA_ASSERT(omc_fileInfo fileInfo, const char* msg)
{
    cerr << fileInfo.filename <<
		"(col " << fileInfo.colStart << "-" << fileInfo.colEnd << ","
		<< "ln " << fileInfo.lineStart << "-" << fileInfo.lineEnd << ")"
		<< endl;
	cerr << msg << endl;
}

/**
 * Implementation of the AdevsSampleData class.
 */
AdevsSampleData::AdevsSampleData(double tStart, double tInterval):
	tStart(tStart),
	tInterval(tInterval),
	n(0),
	enabled(false)
{
}

bool AdevsSampleData::atEvent(double tNow, double eps) const
{
	if (!enabled) return false;
	double tEvent = tStart+double(n)*tInterval;
	return fabs(tEvent-tNow) < eps;
}

double AdevsSampleData::timeToEvent(double tNow) const
{
	double tEvent = tStart+double(n)*tInterval;
	double tToGo = tEvent-tNow;
	if (tToGo < 0.0) return 0.0;
	return tToGo;
}

void AdevsSampleData::update(double tNow, double eps)
{
	if (atEvent(tNow,eps))
		n++;
}

/**
 * Implementation of the AdevsDelayData class.
 */
AdevsDelayData::AdevsDelayData(double maxDelay):
	maxDelay(maxDelay)
{
}

double AdevsDelayData::sample(double t)
{
	assert(t <= traj.back().t);
	if (t <= traj.front().t)
		return traj.front().v;
	// Find two points that bracket t
	list<point_t>::iterator p1, p2 = traj.begin();
	while ((*p2).t <= t)
	{
		p1 = p2;
		p2++;
	}
	assert((*p1).t <= t);
	assert((*p2).t > t);
	double h = (t-((*p1).t))/(((*p2).t) - ((*p1).t));
	return h*((*p2).v)+(1.0-h)*((*p1).v);
}

void AdevsDelayData::insert(double t, double v)
{
	point_t p;
	p.t = t;
	p.v = v;
	assert(traj.empty() || p.t >= traj.back().t);
	if (!traj.empty() &&
		(traj.back().t - traj.front().t > maxDelay) &&
		(t - traj.front().t > maxDelay))
	{
		traj.pop_front();
	}
	traj.push_back(p);
}

/*
 * Implementation of the floor function.
 */
double AdevsFloorFunc::calcValue(double expr)
{
	if (isInInit())
	{
		now = floor(expr);
		below = now - eps;
		above = now + 1.0;
	}
	return now;
}

void AdevsFloorFunc::goUp()
{
	now += 1.0;
	above = now + 1.0;
	below = now - eps;
}

void AdevsFloorFunc::goDown()
{
	above = now + eps;
	now -= 1.0;
	below = now - eps;
}

double AdevsFloorFunc::getZUp(double expr)
{
	return above-expr;
}

double AdevsFloorFunc::getZDown(double expr)
{
	return expr-below;
}

/*
 * Implementation of the ceiling function.
 */
double AdevsCeilFunc::calcValue(double expr)
{
	if (isInInit())
	{
		now = ceil(expr);
		above = now + eps;
		below = now - 1.0;
	}
	return now;
}

void AdevsCeilFunc::goUp()
{
	below = now - eps;
	now += 1.0;
	above = now + eps; 
}

void AdevsCeilFunc::goDown()
{
	now -= 1.0;
	above = now + eps;
	below = now - 1.0;
}

double AdevsCeilFunc::getZUp(double expr)
{
	return above-expr;
}

double AdevsCeilFunc::getZDown(double expr)
{
	return expr-below;
}

/*
 * Implementation of the div function.
 */
double AdevsDivFunc::calcValue(double expr)
{
	if (isInInit())
	{
		now = trunc(expr);
		calc_above_below();
	}
	return now;
}

void AdevsDivFunc::goUp()
{
	now += 1.0;
	calc_above_below();
}

void AdevsDivFunc::goDown()
{
	now -= 1.0;
	calc_above_below();
}

void AdevsDivFunc::calc_above_below()
{
	if (now >= 1.0)
	{
		above = now + 1.0;
		below = now - eps;
	}
	else if (now <= -1.0)
	{
		above = now + eps;
		below = now - 1.0;
	}
	else // now == 0.0
	{
		above = 1.0;
		below = -1.0;
	}
}

double AdevsDivFunc::getZUp(double expr)
{
	return above-expr;
}

double AdevsDivFunc::getZDown(double expr)
{
	return expr-below;
}
