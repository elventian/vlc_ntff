#ifndef NTFF_DIALOG_H
#define NTFF_DIALOG_H

#include <string>
#include <list>
#include <map>
#include <vlc_common.h>

struct extension_dialog_t;
struct extension_widget_t;
struct vlc_object_t;

namespace Ntff {

class Player;
class Feature;
class FeatureList;
class FeatureWidget;
class Widget;
class Button;
class UnmarkedIntervalsWidget;

class Dialog
{
public:
	Dialog(Player *player, FeatureList *featureList);
	void buttonPressed(extension_widget_t *widgetPtr);
	void show();
	void close();
	bool isShown() const { return shown; }
	void updateLength();
private:
	Player *player;
	extension_dialog_t *dialog;
	FeatureList *featureList;
	std::string name;
	std::list<Widget *> widgets;
	std::map<FeatureWidget *, Feature*> features;
	UnmarkedIntervalsWidget *unmarked;
	Button *ok;
	Button *cancel;
	bool shown;
	vlc_timer_t updateLengthTimer;
	bool timerOk;
	
	int getMaxColumn() const;
	bool updateFeatures();
	void appendWidgets(const std::list<Widget *> &other);
	void done();
};

}

#endif // NTFF_DIALOG_H
