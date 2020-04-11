#ifndef NTFF_FEATURE_LIST_H
#define NTFF_FEATURE_LIST_H


#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <vlc_common.h>

namespace Ntff {

struct Interval
{
	Interval(): in(0), out(0){}
	Interval(mtime_t in, mtime_t out, int intensity = 0) : in(in), out(out), intensity(intensity) {}
	bool contains(mtime_t time) const { return time >= in && time < out; }
	mtime_t length() const { return out - in; }
	
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
	std::vector<std::string> getIntervalsIntensity() const;
	int8_t getRecommendedMin() const { return recMin; }
	int8_t getRecommendedMax() const { return recMax; }
	bool setSelected(int8_t min, int8_t max)
	{
		bool res = (selectedMin != min || selectedMax != max);
		selectedMin = min;
		selectedMax = max;
		return res;
	}
	bool setActive(bool activate = true) 
	{
		bool res = (active != activate);
		active = activate;
		return res;
	}
	bool isActive() const { return active; }
private:
	std::string name;
	std::string description;
	int8_t min;
	int8_t max;
	int8_t recMin;
	int8_t recMax;
	int8_t selectedMin;
	int8_t selectedMax;
	bool active;
	std::vector<Interval> intervals;
};

class FeatureList: public std::vector<Feature *>
{
public:
	mtime_t formSelectedIntervals(std::map<mtime_t, Interval> &res, mtime_t len);
	~FeatureList();
	bool appendUnmarked(bool unmarked) {
		bool res = (markedOnly == unmarked);
		markedOnly = !unmarked;
		return res;
	}
private:
	bool markedOnly;

	void insertInterval(std::map<mtime_t, Interval> &container, const Interval& interval) const;
};

}

#endif // NTFF_FEATURE_LIST_H
