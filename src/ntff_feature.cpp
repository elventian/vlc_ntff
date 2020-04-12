#include "ntff_feature.h"
#include <limits>
#include <iostream>

namespace Ntff 
{

Feature::Feature(const std::string &name, const std::string &description, int recMin, int recMax) :
	name(name), description(description),
	recMin(recMin), recMax(recMax), active(true)
{
	min = std::numeric_limits<int8_t>::max();
	max = std::numeric_limits<int8_t>::min();
}

void Feature::appendInterval(const Ntff::Interval &interval) 
{
	intervals.push_back(interval);
	selectedMin = min = std::min(min, interval.intensity);
	selectedMax = max = std::max(min, interval.intensity);
}

bool Feature::isActive(const Interval &interval) const
{
	return interval.intensity >= selectedMin && interval.intensity <= selectedMax;
}

std::vector<std::string> Feature::getIntervalsIntensity() const
{
	std::set<std::string> res;
	for (const Interval &interval: intervals)
	{
		res.insert(std::to_string((int)interval.intensity));
	}
	return std::vector<std::string>(res.begin(), res.end());
}

std::ostream &operator<<(std::ostream &out, const Feature &item)
{
	out << item.name << ", min = " << (int)item.recMin << ", max = " << (int)item.recMax << std::endl;
	return out;
}

mtime_t FeatureList::formSelectedIntervals(std::map<mtime_t, Interval> &res, mtime_t len)
{
	res.clear();
	if (markedOnly) //collect only marked feature intervals
	{
		for (Feature *feature: *this)
		{
			if (!feature->isActive()) { continue; }
			for (const Interval& interval: feature->getIntervals())
			{
				if (feature->isActive(interval))
				{
					insertInterval(res, interval);
				}
			}
		}
	}
	else //find unselected intervals and collect all others
	{
		std::map<mtime_t, Interval> skipIntervals;
		for (Feature *feature: *this)
		{
			for (const Interval& interval: feature->getIntervals())
			{
				if (!feature->isActive() || !feature->isActive(interval))
				{
					insertInterval(skipIntervals, interval);
				}
			}
		}
		
		mtime_t lastUnmarked = 0;
		for (auto &it: skipIntervals)
		{
			Interval &skipInterval = it.second;
			Interval interval(lastUnmarked, skipInterval.in);
			if (interval.length() > 0)
			{
				res[interval.in] = interval;
			}
			lastUnmarked = skipInterval.out;
		}
		
		if (lastUnmarked < len)
		{
			Interval interval(lastUnmarked, len);
			res[interval.in] = interval;
		}
	}
	
	mtime_t length = 0;
	for (auto &it: res)
	{
		Interval &interval = it.second;
		length += (interval.out - interval.in);
	}
	return length;
}

FeatureList::~FeatureList()
{
	for (Feature *feature: *this)
	{
		delete feature;
	}
}

void FeatureList::insertInterval(std::map<mtime_t, Interval> &container, const Interval &interval)
{
	if (container.empty()) { container[interval.in] = interval; return; }
	
	Interval res = interval;
		
	auto faffected = container.lower_bound(interval.in);
	if (faffected == container.end() || 
		(faffected->second.in > interval.in && faffected != container.begin())) //maybe need change prev
	{
		faffected--;
		if (faffected->second.out > interval.in) { res.in = faffected->second.in; }
		res.out = std::max(res.out, faffected->second.out);
		faffected++;
	}
	
	auto laffected = container.lower_bound(interval.out);
	if (laffected != container.end() && laffected->second.in == interval.out)
	{
		res.out = laffected->second.out;
		laffected++;
	}
	else if (laffected != container.begin())
	{
		laffected--;
		res.out = std::max(res.out, laffected->second.out);
		laffected++;
	}
	container.erase(faffected, laffected);
	container[res.in] = res;
}

void FeatureList::removeInterval(std::map<mtime_t, Interval> &container, const Interval &interval)
{
	if (container.empty()) { return; }
		
	auto faffected = container.upper_bound(interval.in);  //maybe need change .out of prev
	faffected--;
	Interval &fint = faffected->second;
	bool done = false;
	if (fint.out > interval.out) 
	{
		container[interval.out] = Interval(interval.out, fint.out);
		done = true;
	}
	if (fint.out > interval.in) 
	{
		fint.out = interval.in; 
	}
	faffected++;
	if (fint.length() == 0) { container.erase(fint.in); }
	if (done) { return; }
	
	auto laffected = container.lower_bound(interval.out);
	Interval shortened;
	if (laffected != container.begin()) //maybe need change .in of prev
	{
		laffected--;
		if (laffected->second.out > interval.out) 
		{
			shortened = laffected->second;
			shortened.in = interval.in;
		}
		laffected++;
	}
	container.erase(faffected, laffected);
	if (shortened.length() != 0)
	{
		container[shortened.in] = shortened;
	}
}


}

