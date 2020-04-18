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
class FeatureList;
class FeatureWidget;
class Widget;
class Button;
class Label;
class ComplexWidget;
class UserAction;

class Dialog
{
public:
	Dialog(Player *player, const FeatureList *featureList);
	~Dialog();
	void buttonPressed(extension_widget_t *widgetPtr);
	void show();
	void hide();
	bool isShown() const { return shown; }
	void applyUserSelection(bool force = false);
private:
	Player *player;
	extension_dialog_t *dialog;
	std::string name;
	std::list<Widget *> widgets;
	std::list<FeatureWidget *> featureWidgets;
	Button *ok;
	Button *cancel;
	Label *playLength;
	bool shown;
	vlc_timer_t updateLengthTimer;
	bool timerOk;
	UserAction *beginAction;
	
	int getMaxColumn() const;
	bool updatedFeatures();
	void appendWidgets(ComplexWidget *src);
	std::string formatTime(mtime_t time) const;
};

}

#endif // NTFF_DIALOG_H
