#ifndef NTFF_DIALOG_H
#define NTFF_DIALOG_H

#include <string>
#include <list>
#include <vlc_common.h>

struct extension_dialog_t;
struct extension_widget_t;
struct vlc_object_t;

namespace Ntff {

class FeatureList;
class FeatureWidget;
class Widget;
class Button;

class Dialog
{
public:
	Dialog(vlc_object_t *obj, const FeatureList *featureList);
	void buttonPressed(extension_widget_t *widgetPtr);
	void close();
	bool isActive() const { return active; }
	bool wait();
private:
	vlc_object_t *obj;
	extension_dialog_t *dialog;
	std::string name;
	std::list<Widget *> widgets;
	std::list<FeatureWidget *> features;
	Button *ok;
	Button *cancel;
	bool active;
	vlc_sem_t sem;
	bool needUpdate;
};

}

#endif // NTFF_DIALOG_H