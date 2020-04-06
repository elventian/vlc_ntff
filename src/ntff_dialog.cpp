#include "ntff_dialog.h"
#include "ntff_feature.h"
#include <vlc_common.h>
#include <vlc_dialog.h>
#include <vlc_extensions.h>
#include <vlc_threads.h>
#include <list>
#include <set>

namespace Ntff {

class Widget
{
public:
	extension_widget_t *getPtr() { return widget; }
	const char *getText() const { return widget->psz_text; }
protected:
	Widget(extension_dialog_t *dialog, extension_widget_type_e type, 
		const std::string &text, int row, int column)
	{
		widget = new extension_widget_t();
		widget->type = type;
		widget->psz_text = new char[text.size() + 1];
		strcpy(widget->psz_text, text.c_str());
		widget->i_row = row + 1; //+1: row and column id should start from 1
		widget->i_column = column + 1;
		widget->p_dialog = dialog;
	}
	extension_widget_t *widget;
};

class Label: public Widget
{
public:
	Label(extension_dialog_t *dialog, const std::string &text, int row, int column):
		Widget(dialog, EXTENSION_WIDGET_LABEL, text, row, column) 
	{}
};

class Checkbox: public Widget
{
public:
	Checkbox(extension_dialog_t *dialog, const std::string &text, int row, int column):
		Widget(dialog, EXTENSION_WIDGET_CHECK_BOX, text, row, column) 
	{}
};

class Combobox: public Widget
{
public:
	Combobox(extension_dialog_t *dialog, const std::string &text, int row, int column, 
		const std::set<std::string> &values):
		Widget(dialog, EXTENSION_WIDGET_DROPDOWN, text, row, column)
	{
		widget->p_values = new extension_widget_t::extension_widget_value_t[values.size()];
		unsigned int id = 0;
		for (const std::string &value: values)
		{
			widget->p_values[id].i_id = id;
			widget->p_values[id].psz_text = new char[value.size() + 1];
			strcpy(widget->p_values[id].psz_text, value.c_str());
			widget->p_values[id].b_selected = false;
			widget->p_values[id].p_next = (id + 1 == values.size()) ? nullptr : &widget->p_values[id + 1];
			id++;
		}
	}
};

class Button: public Widget
{
	public: 
	Button(extension_dialog_t *dialog, const std::string &text, int row, int column):
		Widget(dialog, EXTENSION_WIDGET_BUTTON, text, row, column) {}
};

class FeatureWidget
{
public:
	FeatureWidget(extension_dialog_t *dialog, const Feature *feature, int row)
	{
		Label *name = new Label(dialog, feature->getName(), row, widgets.size());
		widgets.push_back(name);
		
		Label *minLabel = new Label(dialog, "min: " , row, widgets.size());
		widgets.push_back(minLabel);
		
		std::string recMin = std::to_string(feature->getRecommendedMin());
		intensityMin = new Combobox(dialog, recMin, 
			row, widgets.size(), feature->getIntervalsIntensity());
		widgets.push_back(intensityMin);
		
		Label *maxLabel = new Label(dialog, "max: " , row, widgets.size());
		widgets.push_back(maxLabel);
		
		std::string recMax = std::to_string(feature->getRecommendedMax());
		intensityMax = new Combobox(dialog, recMax, 
			row, widgets.size(), feature->getIntervalsIntensity());
		widgets.push_back(intensityMax);
	}
	
	const std::list<Widget *> &getWidgets() const { return widgets; }
	
	void getSelectedIntensity(int8_t &min, int8_t &max)
	{
		min = (int8_t)atoi(intensityMin->getText());
		max = (int8_t)atoi(intensityMax->getText());
	}
private:
	std::list<Widget *> widgets;
	Combobox *intensityMin;
	Combobox *intensityMax;
};

static int DialogCallback(vlc_object_t *, char const *, vlc_value_t, vlc_value_t newval, void *data)
{
	Dialog *dialog = (Dialog *)data;
    extension_dialog_command_t *command = (extension_dialog_command_t *)newval.p_address;
	
	if (command->event == EXTENSION_EVENT_CLICK)
	{
		dialog->buttonPressed((extension_widget_t *)command->p_data);
	}
	else if (command->event == EXTENSION_EVENT_CLOSE)
	{
		dialog->close();
	}
	
    return VLC_SUCCESS;
}

Dialog::Dialog(vlc_object_t *obj, FeatureList *featureList) : obj(obj)
{
	name = "Ntff Settings";
	dialog = new extension_dialog_t();
	dialog->p_object = obj;
	dialog->psz_title = (char *)name.c_str();
	dialog->p_sys = NULL;
	dialog->p_sys_intf = NULL;
	dialog->b_hide = false;
	dialog->b_kill = false;
	
	int row = 0;
	for (Feature *feature: *featureList)
	{
		FeatureWidget *fwidget = new FeatureWidget(dialog, feature, row);
		features[fwidget] = feature;
		widgets.insert(widgets.end(), fwidget->getWidgets().begin(), fwidget->getWidgets().end());
		row++;
		
		msg_Dbg(obj, "~~~~~feature name len = %li", feature->getName().size());
	}
	
	ok = new Button(dialog, "OK", row, 0);
	widgets.push_back(ok);
	
	cancel = new Button(dialog, "Cancel", row, getMaxColumn());
	widgets.push_back(cancel);
	
	dialog->widgets.i_size = widgets.size();
	dialog->widgets.p_elems = new extension_widget_t *[dialog->widgets.i_size];
	int id = 0;
	for (Widget *widget: widgets)
	{
		dialog->widgets.p_elems[id] = widget->getPtr();
		id++;
	}
	
	vlc_ext_dialog_update(obj, dialog);
	var_Create(obj, "dialog-event", VLC_VAR_ADDRESS);
	var_AddCallback(obj, "dialog-event", DialogCallback, this);
	
	active = true;
	needUpdate = false;
	vlc_sem_init(&sem, 0);
}

void Dialog::buttonPressed(extension_widget_t *widgetPtr)
{
	if (widgetPtr == ok->getPtr()) //confirm
	{
		for (auto p: features)
		{
			FeatureWidget *widget = p.first;
			Feature *feature = p.second;
			int8_t min, max;
			widget->getSelectedIntensity(min, max);
			feature->setSelected(min, max);
		}
		needUpdate = true;
		close();
	}
	else if (widgetPtr == cancel->getPtr()) //cancel
	{
		close();
	}
}

void Dialog::close() 
{
	active = false;
	vlc_sem_post(&sem);
}

bool Dialog::wait()
{
	if (active)
	{
		vlc_sem_wait(&sem);
	}
	
	if (dialog->b_hide == false)
	{
		dialog->b_hide = true;
		vlc_ext_dialog_update(obj, dialog); //need to hide from other thread than DialogCallback
		if (needUpdate)
		{
			needUpdate = false;
			return true;
		}
	}
	
	return false;
}

int Dialog::getMaxColumn() const
{
	int res = 0;
	for (Widget *w: widgets)
	{
		res = std::max(w->getPtr()->i_column, res);
	}
	return res - 1;
}


}
