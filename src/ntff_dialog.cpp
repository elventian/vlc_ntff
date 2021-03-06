#include "ntff_dialog.h"
#include "ntff_feature.h"
#include "ntff_player.h"
#include <vlc_common.h>
#include <vlc_dialog.h>
#include <vlc_extensions.h>
#include <vlc_threads.h>
#include <list>
#include <set>
#include <limits>
#include <algorithm>

namespace Ntff {

class Widget
{
	friend class ComplexWidget;
public:
	extension_widget_t *getPtr() { return widget; }
	const char *getText() const { return widget->psz_text; }
	int getValue() const { return atoi(widget->psz_text); }
	void updateText(const std::string &text)
	{
		delete widget->psz_text;
		setText(text);
		widget->b_update = true;
	}
	int getRow() const { return widget->i_row; }
	virtual bool changed()
	{
		if (strcmp(prevText.c_str(), widget->psz_text) != 0)
		{
			prevText = std::string(widget->psz_text);
			return true;
		}
		return false;
	}
protected:
	void setText(const std::string &text) 
	{
		widget->psz_text = new char[text.size() + 1];
		strcpy(widget->psz_text, text.c_str());
		prevText = text;
		widget->b_update = true;
	}
	
	Widget(extension_dialog_t *dialog, extension_widget_type_e type, 
		const std::string &text, int row, int column)
	{
		widget = new extension_widget_t();
		widget->type = type;
		setText(text); 
		widget->i_row = row + 1; //+1: row and column id should start from 1
		widget->i_column = (column == -1) ? -1 : column + 1;
		widget->p_dialog = dialog;
	}
	extension_widget_t *widget;
	std::string prevText;
};

class Label: public Widget
{
public:
	Label(extension_dialog_t *dialog, const std::string &text, int row, int column = -1):
		Widget(dialog, EXTENSION_WIDGET_LABEL, text, row, column), text(text) {}
	
	void setColor(const std::string &hashCode)
	{
		setText("<font color=\"" + hashCode + "\">" + text + "</font>");
	}
private:
	std::string text;
};

class Checkbox: public Widget
{
public:
	Checkbox(extension_dialog_t *dialog, const std::string &text, bool checked, int row, int column = -1):
		Widget(dialog, EXTENSION_WIDGET_CHECK_BOX, text, row, column) 
	{
		widget->b_checked = checked;
		prevChecked = checked;
	}
	bool isChecked() const { return widget->b_checked; }
	bool changed() override
	{
		if (prevChecked != widget->b_checked)
		{
			prevChecked = widget->b_checked;
			return true;
		}
		return false;
	}
private:
	bool prevChecked;
};

class Combobox: public Widget
{
public:
	Combobox(extension_dialog_t *dialog, const std::string &text, const std::vector<std::string> &values, 
		int row, int column = -1):
		Widget(dialog, EXTENSION_WIDGET_DROPDOWN, text, row, column)
	{
		fillValues(values);
	}
	
	int getSelectedId() const
	{
		extension_widget_t::extension_widget_value_t *cur = widget->p_values;
		while (cur)
		{
			if (strcmp(cur->psz_text, widget->psz_text) == 0)
			{
				return cur->i_id;
			}
			cur = cur->p_next;
		}
		return 0;
	}
protected:
	void fillValues(const std::vector<std::string> &values)
	{
		if (values.empty()) { widget->p_values = nullptr; return; }
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
	Button(extension_dialog_t *dialog, const std::string &text, int row, int column = -1):
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
	void addWidget(Widget *w) {
		if (w->widget->i_column == -1) 
		{ 
			w->widget->i_column = widgets.size() + 1; 
		}
		widgets.push_back(w); 
	}
};

class UserAction: public Combobox
{
public:
	enum Action {Add, Remove};
	UserAction(extension_dialog_t *dialog, Action selected, int row, int column = -1):
		Combobox(dialog, "", std::vector<std::string>(), row, column)
	{
		values.push_back("add");
		values.push_back("remove");
		fillValues(values);
		setText(values[selected]);
	}
	void setAction(Action action)
	{
		updateText(values[action]);
	}
	Action getAction() const { return (Action)getSelectedId(); }
	static Action fromStr(const std::string &action)
	{
		if (action == "add") { return Add; }
		return Remove;
	}
	static std::string toStr(Action action)
	{
		if (action == Add) { return "add"; }
		return "remove";
	}
private:
	std::vector<std::string> values;
};

class FeatureWidget: public ComplexWidget
{
public:
	enum {Less, More, LessOrEq, MoreOrEq};
	FeatureWidget(extension_dialog_t *dialog, Feature *feature, int row): feature(feature)
	{
		addWidget(new Label(dialog, "then", row));
		
		action = new UserAction(dialog, UserAction::Add, row);
		addWidget(action);
		
		addWidget(new Label(dialog, "intervals where", row));
		
		name = new Label(dialog, feature->getName(), row);
		addWidget(name);
		
		eqStr.push_back("<");
		eqStr.push_back(">");
		eqStr.push_back("≤");
		eqStr.push_back("≥");
		
		equality = new Combobox(dialog, "", eqStr, row);
		addWidget(equality);
		
		value = new Combobox(dialog, "", feature->getIntervalsIntensity(), row);
		addWidget(value);
		
		unmarked = new Checkbox(dialog, "", false, row);
		addWidget(unmarked);
		unmarkedLabel = new Label(dialog, "or not set", row);
		addWidget(unmarkedLabel);
		
		restore();
	}
	
	void getSelectedIntensity(int8_t &min, int8_t &max)
	{
		int eq = equality->getSelectedId();
		int8_t intensity = (int8_t)value->getValue();
		if (eq == Less || eq == LessOrEq)
		{
			min = std::numeric_limits<int8_t>::min();
			max = intensity;
			if (eq == Less) { max--; }
		}
		else
		{
			max = std::numeric_limits<int8_t>::max();
			min = intensity;
			if (eq == More) { min++; }
		}
	}
	
	bool update()
	{
		bool res = false;
		if (action->changed()) { updateNameColor(); res = true; }
		if (unmarked->changed()) { updateUnmarkedColor(); res = true;}
		if (equality->changed()) { res = true; }
		if (value->changed()) { res = true;}
		return res;
	}
	int getRow() const { return name->getRow(); }
	bool addCmd() const { return action->getAction() == UserAction::Add; }
	bool affectUnmarked() const { return unmarked->isChecked(); }
	void save() const
	{
		feature->setRecommended(atoi(value->getText()), 
			UserAction::toStr(action->getAction()), 
			equality->getText());
	}
	void restore()
	{
		value->updateText(std::to_string(feature->getRecIntensity()));
		std::string eq = (std::find(eqStr.begin(), eqStr.end(), feature->getEq()) != eqStr.end()) ? 
			feature->getEq() : eqStr[0];
		equality->updateText(eq);
		action->setAction(UserAction::fromStr(feature->getAction()));
		updateNameColor();
		updateUnmarkedColor();
	}
	Feature *getFeature() const { return feature; }
private:
	Feature *feature;
	Label *name;
	UserAction *action;
	Combobox *equality;
	Combobox *value;
	Checkbox *unmarked;
	Label *unmarkedLabel;
	std::vector<std::string> eqStr;
	
	void updateNameColor()
	{
		if (action->getAction() == UserAction::Add) { name->setColor("#239b56");}
		else { name->setColor("#ff495a");}
	}
	
	void updateUnmarkedColor()
	{
		if (unmarked->isChecked()) { unmarkedLabel->setColor("#000000");}
		else { unmarkedLabel->setColor("#abb2b9");}
	}
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
		dialog->hide();
	}
	
    return VLC_SUCCESS;
}

static void TimerCallback(void *ptr)
{
	((Dialog *)ptr)->applyUserSelection();
}

Dialog::Dialog(Player *player, const FeatureList *featureList) : 
	player(player), shown(false)
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
	
	widgets.push_back(new Label(dialog, "First", row, 0));
	beginAction = new UserAction(dialog, UserAction::Remove, row, 1);
	widgets.push_back(beginAction);
	widgets.push_back(new Label(dialog, "everything", row, 2));
	
	row++;
	
	for (Feature *feature: *featureList)
	{
		FeatureWidget *fwidget = new FeatureWidget(dialog, feature, row);
		featureWidgets.push_back(fwidget);
		appendWidgets(fwidget);
		row++;
		
		msg_Dbg(player->getVlcObj(), "~~~~~feature name len = %li", feature->getName().size());
	}
	
	playLength = new Label(dialog, formatTime(player->getLength()), row++, 0);
	widgets.push_back(playLength);
	
	cancel = new Button(dialog, "Cancel", row, 0);
	widgets.push_back(cancel);
	
	ok = new Button(dialog, "OK", row, getMaxColumn());
	widgets.push_back(ok);
	
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
	timerOk = vlc_timer_create(&updateLengthTimer, TimerCallback, this) == VLC_SUCCESS;
	if (!timerOk) { msg_Warn(player->getVlcObj(), "Unable to create dialog timer"); }
}

Dialog::~Dialog()
{
	var_DelCallback(player->getVlcObj(), "dialog-event", DialogCallback, this);
	dialog->b_kill = true;
	vlc_ext_dialog_update(player->getVlcObj(), dialog);
	if (timerOk) { vlc_timer_destroy(updateLengthTimer); }
}

void Dialog::buttonPressed(extension_widget_t *widgetPtr)
{
	if (widgetPtr == ok->getPtr())
	{
		if (timerOk) { vlc_timer_schedule(updateLengthTimer, false, 0, 0); }
		for (FeatureWidget *widget: featureWidgets)	{ widget->save(); }
	}
	else if (widgetPtr == cancel->getPtr())
	{
		if (timerOk) { vlc_timer_schedule(updateLengthTimer, false, 0, 0); }
		for (FeatureWidget *widget: featureWidgets)	{ widget->restore(); }
	}
	player->setIntervalsSelected();
}

void Dialog::show()
{
	dialog->b_hide = false;
	applyUserSelection(true);
	shown = true;
	
	if (timerOk) { vlc_timer_schedule(updateLengthTimer, false, CLOCK_FREQ/4, CLOCK_FREQ/4); }
}

void Dialog::hide() //need to hide from other thread than DialogCallback
{
	if (shown == true)
	{
		shown = false;
		dialog->b_hide = true;
		vlc_ext_dialog_update(player->getVlcObj(), dialog);
	}
}

void Dialog::appendWidgets(ComplexWidget *src) 
{
	widgets.insert(widgets.end(), src->getWidgets().begin(), src->getWidgets().end());
}

std::string Dialog::formatTime(mtime_t time) const //copied from StreamTime::formatTime
{
	int seconds = time / 1000000;
	char psz_time[MSTRTIME_MAX_SIZE];
	snprintf( psz_time, MSTRTIME_MAX_SIZE, "%d:%02d:%02d",
			  (int) (seconds / (60 * 60)),
			  (int) (seconds / 60 % 60),
			  (int) (seconds % 60) );
    return std::string(psz_time);
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

bool Dialog::updatedFeatures()
{
	bool updated = false;
	for (FeatureWidget *widget: featureWidgets)
	{
		if (widget->update()) { updated = true; }
	}
	return updated;
}

void Dialog::applyUserSelection(bool force)
{
	bool beginChanged = beginAction->changed();
	if (updatedFeatures() || beginChanged || force)
	{
		player->lockIntervals(true);
		player->resetIntervals(beginAction->getAction() == UserAction::Remove);
		std::map<int, FeatureWidget *> orderedFeatures;
		for (FeatureWidget *widget: featureWidgets)
		{
			orderedFeatures[widget->getRow()] = widget;
		}
		
		for (auto p: orderedFeatures)
		{
			FeatureWidget *widget = p.second;
			int8_t min, max;
			widget->getSelectedIntensity(min, max);
			player->modifyIntervals(widget->addCmd(), widget->getFeature(), min, max, widget->affectUnmarked());
		}
		
		player->recalcLength();
		mtime_t length = player->getLength();
		playLength->updateText(formatTime(length));
		vlc_ext_dialog_update(player->getVlcObj(), dialog);
		player->updateCurrentInterval();
		player->lockIntervals(false);
	}
}


}
