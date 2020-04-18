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
public:
	Feature(const std::string &name, const std::string &description, 
		const std::string &recAction, const std::string &recEq, int8_t recIntensity);
	void appendInterval(const Interval &interval);
	const std::vector<Interval> &getIntervals() const { return intervals; }
	const std::string &getName() const { return name; }
	const std::string &getDescription() const { return description; }
	const std::string &getAction() const { return recAction; }
	const std::string &getEq() const { return recEq; }
	int8_t getRecIntensity() const { return recIntensity; }
	std::vector<std::string> getIntervalsIntensity() const;
	void setRecommended(int8_t intensity, const std::string &action, const std::string &eq)
	{
		recIntensity = intensity;
		recAction = action;
		recEq = eq;
	}
private:
	std::string name;
	std::string description;
	int8_t recIntensity;
	std::string recAction;
	std::string recEq;
	int8_t min;
	int8_t max;
	std::vector<Interval> intervals;
};

class FeatureList: public std::vector<Feature *>
{
public:
	~FeatureList();
	static void insertInterval(std::map<mtime_t, Interval> &container, const Interval& interval);
	static void removeInterval(std::map<mtime_t, Interval> &container, const Interval& interval);
};

}

#endif // NTFF_FEATURE_LIST_H
