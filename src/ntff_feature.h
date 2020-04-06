#ifndef NTFF_FEATURE_LIST_H
#define NTFF_FEATURE_LIST_H


#include <string>
#include <vector>
#include <map>
#include <set>
#include <vlc_common.h>

namespace Ntff {

struct Interval
{
	Interval(){}
	Interval(mtime_t in, mtime_t out, int intensity) : in(in), out(out), intensity(intensity) {}
	bool contains(mtime_t time) const { return time >= in && time < out; }
	
	mtime_t in;
	mtime_t out;
	int8_t intensity;
};

class Feature
{
	friend std::ostream &operator<<(std::ostream &out, const Feature &item);
public:
	Feature(const std::string &name, const std::string &description, int recMin, int recMax);
	void appendInterval(const Interval &interval);
	const std::vector<Interval> &getIntervals() { return intervals; }
	bool isActive(const Interval &interval) const;
	const std::string &getName() const { return name; }
	const std::string &getDescription() const { return description; }
	std::set<std::string> getIntervalsIntensity() const;
	int8_t getRecommendedMin() const { return recMin; }
	int8_t getRecommendedMax() const { return recMax; }
	void setSelected(int8_t min, int8_t max)
	{
		selectedMin = min;
		selectedMax = max;
	}
private:
	std::string name;
	std::string description;
	int8_t min;
	int8_t max;
	int8_t recMin;
	int8_t recMax;
	int8_t selectedMin;
	int8_t selectedMax;
	std::vector<Interval> intervals;
};

class FeatureList: public std::vector<Feature *>
{
public:
	mtime_t formSelectedIntervals(std::map<mtime_t, Interval> &res);
	~FeatureList();
};

}

#endif // NTFF_FEATURE_LIST_H
