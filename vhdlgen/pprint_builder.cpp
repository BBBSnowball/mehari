#include "pprint_builder.h"

namespace pprint {

PrettyPrintBuilder::PrettyPrintBuilder() { }

PrettyPrintBuilder::~PrettyPrintBuilder() { }

bool PrettyPrintBuilder::hasCurrentContainer() {
	return !containerStack.empty();
}

unsigned int PrettyPrintBuilder::depth() {
	return containerStack.size();
}

PrettyPrintedWithChildren_p PrettyPrintBuilder::currentContainer() {
	assert(hasCurrentContainer());

	if (hasCurrentContainer())
		return containerStack.top();
	else
		return PrettyPrintedWithChildren_p();
}

PrettyPrintBuilder& PrettyPrintBuilder::up() {
	assert(hasCurrentContainer());

	containerStack.top()->measure();
	containerStack.pop();

	return *this;
}

PrettyPrintBuilder& PrettyPrintBuilder::addAndSelect(PrettyPrintedWithChildren_p container) {
	if (hasCurrentContainer())
		currentContainer()->add(container);
	else {
		assert(!_root);
		_root = container;
	}

	containerStack.push(container);
	_recentItem = container;

	return *this;
}

PrettyPrintBuilder& PrettyPrintBuilder::addAndSelect(PrettyPrintedWithChildren* container) {
	return addAndSelect(PrettyPrintedWithChildren_p(container));
}

PrettyPrinted_p PrettyPrintBuilder::recentItem() {
	assert(_recentItem);

	return _recentItem;
}

PrettyPrintBuilder& PrettyPrintBuilder::add(PrettyPrinted_p item) {
	if (!hasCurrentContainer())
		// add default container
		append();

	currentContainer()->add(item);
	_recentItem = item;
	return *this;
}

PrettyPrintBuilder& PrettyPrintBuilder::add(PrettyPrinted* item) {
	return add(PrettyPrinted_p(item));
}

PrettyPrintBuilder& PrettyPrintBuilder::add(std::string text) {
	return add(Text::create(text));
}

PrettyPrintBuilder& PrettyPrintBuilder::columns() {
	return addAndSelect(new HCat());
}

PrettyPrintBuilder& PrettyPrintBuilder::append() {
	return addAndSelect(new VCat());
}

PrettyPrinted_p PrettyPrintBuilder::build() {
	assert(_root);

	while (hasCurrentContainer())
		up();

	PrettyPrintedWithChildren_p root = _root;

	_root.reset();

	return root;
}

} // end of namespace pprint
