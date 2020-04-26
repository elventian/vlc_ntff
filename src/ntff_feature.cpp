#include "ntff_feature.h"
#include <limits>
#include <iostream>

namespace Ntff 
{

Feature::Feature(const std::string &name, const std::string &description, 
	const std::string &recAction, const std::string &recEq, int8_t recIntensity):
	name(name), description(description),
	recIntensity(recIntensity), recAction(recAction), recEq(recEq)
{
	min = std::numeric_limits<int8_t>::max();
	max = std::numeric_limits<int8_t>::min();
}

void Feature::appendInterval(const Ntff::Interval &interval) 
{
	intervals.push_back(interval);
	min = std::min(min, interval.intensity);
	max = std::max(min, interval.intensity);
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

FeatureList::~FeatureList()
{
	for (Feature *feature: *this)
	{
		delete feature;
	}
}

void FeatureList::insertInterval(std::map<frame_id, Interval> &container, const Interval &interval)
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

void FeatureList::removeInterval(std::map<frame_id, Interval> &container, const Interval &interval)
{
	if (container.empty()) { return; }
	
	auto faffected = container.upper_bound(interval.in);  //maybe need change .out of prev
	if (faffected == container.begin()) { return; }
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

