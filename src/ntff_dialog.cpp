#include "ntff_dialog.h"
#include "ntff_feature.h"
#include "ntff_player.h"
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
	void setText(const std::string &text) 
	{
		widget->psz_text = new char[text.size() + 1];
		strcpy(widget->psz_text, text.c_str());
	}
	
	Widget(extension_dialog_t *dialog, extension_widget_type_e type, 
		const std::string &text, int row, int column)
	{
		widget = new extension_widget_t();
		widget->type = type;
		setText(text); 
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
	Checkbox(extension_dialog_t *dialog, const std::string &text, bool checked, int row, int column):
		Widget(dialog, EXTENSION_WIDGET_CHECK_BOX, text, row, column) 
	{
		widget->b_checked = checked;
	}
	
	bool isChecked() const { return widget->b_checked; }
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

class ComplexWidget
{
public:
	const std::list<Widget *> &getWidgets() const { return widgets; }
protected:
	ComplexWidget() {}
	std::list<Widget *> widgets;
	int getWidgetsNum() const { return widgets.size(); }
	void addWidget(Widget *w) { widgets.push_back(w); }
};

class FeatureWidget: public ComplexWidget
{
public:
	FeatureWidget(extension_dialog_t *dialog, const Feature *feature, int row)
	{
		addWidget(new Label(dialog, feature->getName(), row, getWidgetsNum()));
		
		active = new Checkbox(dialog, feature->getName(), true, row, getWidgetsNum());
		addWidget(active);
		
		addWidget(new Label(dialog, "min: " , row, getWidgetsNum()));
		
		std::string recMin = std::to_string(feature->getRecommendedMin());
		intensityMin = new Combobox(dialog, recMin, 
			row, getWidgetsNum(), feature->getIntervalsIntensity());
		addWidget(intensityMin);
		
		addWidget(new Label(dialog, "max: " , row, getWidgetsNum()));
		
		std::string recMax = std::to_string(feature->getRecommendedMax());
		intensityMax = new Combobox(dialog, recMax, 
			row, getWidgetsNum(), feature->getIntervalsIntensity());
		addWidget(intensityMax);
	}
	
	void getSelectedIntensity(int8_t &min, int8_t &max)
	{
		min = (int8_t)atoi(intensityMin->getText());
		max = (int8_t)atoi(intensityMax->getText());
	}
	
	bool isActive() const { return active->isChecked(); }
private:
	Combobox *intensityMin;
	Combobox *intensityMax;
	Checkbox *active;
};

class UnmarkedIntervalsWidget: public ComplexWidget
{
public:
	UnmarkedIntervalsWidget(extension_dialog_t *dialog, int row)
	{
		addWidget(new Label(dialog, "Other", row, getWidgetsNum()));
		active = new Checkbox(dialog, "", true, row, getWidgetsNum());
		addWidget(active);
	}
	bool isActive() const { return active->isChecked(); }
private:
	Checkbox *active;
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


Dialog::Dialog(Player *player, FeatureList *featureList) : 
	player(player), featureList(featureList), shown(false)
{
	name = "Ntff Settings";
	dialog = new extension_dialog_t();
	dialog->p_object = player->getVlcObj();
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
		
		msg_Dbg(player->getVlcObj(), "~~~~~feature name len = %li", feature->getName().size());
	}
	
	unmarked = new UnmarkedIntervalsWidget(dialog, row++);
	appendWidgets(unmarked->getWidgets());
	
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
	
	var_Create(player->getVlcObj(), "dialog-event", VLC_VAR_ADDRESS);
	var_AddCallback(player->getVlcObj(), "dialog-event", DialogCallback, this);
}

void Dialog::buttonPressed(extension_widget_t *widgetPtr)
{
	if (widgetPtr == ok->getPtr()) //confirm
	{
		updateFeatures();
		done();
	}
	else if (widgetPtr == cancel->getPtr()) //cancel
	{
		done();
	}
}

void Dialog::show()
{
	dialog->b_hide = false;
	vlc_ext_dialog_update(player->getVlcObj(), dialog);
	shown = true;
	
	auto timerCallback = [](void *ptr)
	{
		((Dialog *)ptr)->updateLength();
	};

	timerOk = vlc_timer_create(&updateLengthTimer, timerCallback, this) == VLC_SUCCESS;
	if (timerOk) { vlc_timer_schedule(updateLengthTimer, false, CLOCK_FREQ/4, CLOCK_FREQ/4); }
	else { msg_Warn(player->getVlcObj(), "Unable to create dialog timer"); }
}

void Dialog::close() 
{
	if (shown == true)
	{
		shown = false;
		dialog->b_hide = true;
		vlc_ext_dialog_update(player->getVlcObj(), dialog); //need to hide from other thread than DialogCallback
	}
}

void Dialog::appendWidgets(const std::list<Widget *> &other) 
{
	widgets.insert(widgets.end(), other.begin(), other.end());
}

void Dialog::done()
{
	if (timerOk) { vlc_timer_destroy(updateLengthTimer); }
	player->setIntervalsSelected();
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

bool Dialog::updateFeatures()
{
	bool updated = false;
	for (auto p: features)
	{
		FeatureWidget *widget = p.first;
		Feature *feature = p.second;
		int8_t min, max;
		widget->getSelectedIntensity(min, max);
		updated |= feature->setSelected(min, max);
		updated |= feature->setActive(widget->isActive());
	}
	updated |= featureList->appendUnmarked(unmarked->isActive());
	return updated;
}

void Dialog::updateLength()
{
	if (updateFeatures())
	{
		player->updatePlayIntervals();
	}
}


}
