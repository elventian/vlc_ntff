#include "ntff_feature.h"
#include <limits>
#include <iostream>

namespace Ntff 
{

Feature::Feature(const std::string &name, const std::string &description, int recMin, int recMax) :
	name(name), description(description),
	recMin(recMin), recMax(recMax), 
	curInterval(0) 
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

std::ostream &operator<<(std::ostream &out, const Feature &item)
{
	out << item.name << ", min = " << (int)item.recMin << ", max = " << (int)item.recMax << std::endl;
	return out;
}


}

