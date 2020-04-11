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

void FeatureList::insertInterval(std::map<mtime_t, Interval> &container, const Interval &interval) const
{
	auto it = container.lower_bound(interval.in);
	if (it == container.end())
	{
		container[interval.in] = interval;
	}
	else
	{
		Interval selected = it->second; //next after interval
		if (interval.out >= selected.in)
		{
			selected.in = interval.in;
			selected.out = std::max(interval.out, selected.out);
			container[interval.in] = selected;
			container.erase(it->first);
			return;
		}
		
		if (it != container.begin()) //prev
		{
			it--;
			Interval &selected = it->second;
			if (interval.in <= selected.out)
			{
				if (interval.out > selected.out) { selected.out = interval.out; }
				return;
			}
		}
		container[interval.in] = interval;
	}
}


}

