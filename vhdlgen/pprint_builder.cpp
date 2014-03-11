#include "pprint_builder.h"

#include <boost/scoped_ptr.hpp>
#include <list>

namespace pprint {

struct _PrettyPrintBuilderStackItem {
	PrettyPrintedWithChildren_p container;

	std::list<PrettyPrinted_p> seperatedBy;
	bool first;
	PrettyPrintedWithChildren_p pendingContainer;

	_PrettyPrintBuilderStackItem(PrettyPrintedWithChildren_p container) : container(container), first(true) { }

	void add(PrettyPrinted_p item) {
		maybeAddSeperatorBefore(item);

		container->add(item);
	}

	void addContainer(PrettyPrintedWithChildren_p container) {
		// Defer adding the container because we have to know
		// whether it will be empty.
		pendingContainer = container;
	}

	void onPop() {
		container->measure();
	}

	void onChildContainerFinished() {
		maybeAddSeperatorBefore(pendingContainer);

		container->add(pendingContainer);
	}

	void maybeAddSeperatorBefore(PrettyPrinted_p item) {
		if (isNotEmpty(item)) {
			if (first)
				first = false;
			else {
				for (std::list<PrettyPrinted_p>::iterator iter = seperatedBy.begin(); iter != seperatedBy.end(); ++iter)
					container->add(*iter);
			}
		}
	}

	bool isNotEmpty(PrettyPrinted_p item) {
		boost::scoped_ptr<LineIterator> lines(item->lines());
		bool not_empty = lines->next();
		std::cout << "item " << item << " is " << (not_empty ? "not " : "") << "empty." << std::endl;
		return not_empty;
	}
};

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
		return containerStack.top().container;
	else
		return PrettyPrintedWithChildren_p();
}

PrettyPrintBuilder& PrettyPrintBuilder::up() {
	assert(hasCurrentContainer());

	containerStack.top().onPop();
	containerStack.pop();

	if (!containerStack.empty())
		containerStack.top().onChildContainerFinished();

	return *this;
}

PrettyPrintBuilder& PrettyPrintBuilder::addAndSelect(PrettyPrintedWithChildren_p container) {
	if (hasCurrentContainer())
		containerStack.top().addContainer(container);
	else {
		assert(!_root);
		_root = container;
	}

	containerStack.push(_PrettyPrintBuilderStackItem(container));
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

_PrettyPrintBuilderStackItem& PrettyPrintBuilder::getOrCreateCurrent() {
	if (!hasCurrentContainer())
		// add default container
		append();

	return containerStack.top();
}

PrettyPrintBuilder& PrettyPrintBuilder::add(PrettyPrinted_p item) {
	getOrCreateCurrent().add(item);
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

PrettyPrintBuilder& PrettyPrintBuilder::appendOverlapping() {
	return addAndSelect(new VCatOverlapping());
}

PrettyPrinted_p PrettyPrintBuilder::build() {
	assert(_root);

	while (hasCurrentContainer())
		up();

	PrettyPrintedWithChildren_p root = _root;

	_root.reset();

	return root;
}

PrettyPrintBuilder& PrettyPrintBuilder::add(const PrettyPrintable& item) {
	return add(item.prettyPrint());
}

PrettyPrintBuilder& PrettyPrintBuilder::add(const PrettyPrintable* item) {
	return add(item->prettyPrint());
}

PrettyPrintBuilder& PrettyPrintBuilder::seperateBy(PrettyPrinted_p item) {
	getOrCreateCurrent().seperatedBy.push_back(item);

	return *this;
}

PrettyPrintBuilder& PrettyPrintBuilder::seperateBy(std::string text) {
	return seperateBy(Text::create(text));
}

} // end of namespace pprint
