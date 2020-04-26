#ifndef NTFF_FEATURE_LIST_H
#define NTFF_FEATURE_LIST_H


#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <math.h>
#include <vlc_common.h>

namespace Ntff {

using frame_id = int64_t;

struct Interval
{
	Interval(): in(0), out(0){}
	Interval(frame_id in, frame_id out, int intensity = 0) : in(in), out(out), intensity(intensity) {}
	bool contains(frame_id frame) const { return frame >= in && frame < out; }
	frame_id length() const { return out - in; }
	
	frame_id in;
	frame_id out;
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
	static void insertInterval(std::map<frame_id, Interval> &container, const Interval& interval);
	static void removeInterval(std::map<frame_id, Interval> &container, const Interval& interval);
};

}

#endif // NTFF_FEATURE_LIST_H
