#include "XMLFilter.h"

#include "ifilter.h"

namespace filters {

XMLFilter::XMLFilter(const std::string& name, bool readOnly) :
	_name(name),
	_readonly(readOnly)
{
	updateEventName();
}

// Test visibility of an item against all rules
bool XMLFilter::isVisible(const std::string& item, const std::string& name) const {

	// Iterate over the rules in this filter, checking if each one is a rule for
	// the chosen item. If so, test the match expression and retrieve the visibility
	// flag if there is a match.

	bool visible = true; // default if unmodified by rules

	for (FilterRules::const_iterator ruleIter = _rules.begin();
		 ruleIter != _rules.end();
		 ++ruleIter)
	{
		// Check the item type.
		if (ruleIter->type != item)
			continue;

		if (item == "surfaceflags") {
			const int flags = string::toInt(name);
			visible = !(flags & ruleIter->surfaceflags);
		} else if (item == "contentflags") {
			const int flags = string::toInt(name);
			visible = !(flags & ruleIter->contentflags);
		} else {
			// If we have a rule for this item, use the wildcard matcher to match the query name
			// against the "match" parameter
			if (string::matchesWildcard(name, ruleIter->match)) {
				// Overwrite the visible flag with the value from the rule.
				visible = ruleIter->show;
			}
		}
	}

	// Pass back the current visibility value
	return visible;
}

// The command target
void XMLFilter::toggle(bool newState)
{
	GlobalFilterSystem().setFilterState(_name, newState);
}

std::string XMLFilter::getEventName() const {
	return _eventName;
}

void XMLFilter::setName(const std::string& newName) {
	// Set the name ...
	_name = newName;

	// ...and update the event name
	updateEventName();
}

void XMLFilter::onToggle ()
{
	// Allocate a reference, otherwise the call to GlobalFilterSystem() will crash
	GlobalFilterModuleRef ref;
	bool currentState = GlobalFilterSystem().getFilterState(_name);
	GlobalFilterSystem().setFilterState(_name, !currentState);
}

bool XMLFilter::isReadOnly() const {
	return _readonly;
}

FilterRules XMLFilter::getRuleSet() {
	return _rules;
}

void XMLFilter::setRules(const FilterRules& rules) {
	_rules = rules;
}

void XMLFilter::updateEventName() {
	// Construct the eventname out of the filtername (strip the spaces and add "Filter" prefix)
	_eventName = _name;
	_eventName = string::eraseAllSpaces(_eventName);
	_eventName = "Filter" + _eventName;
}

} // namespace filters
