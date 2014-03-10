#ifndef __HEADER_PPRINT_BUILDER__
#define __HEADER_PPRINT_BUILDER__

#include "pprint.h"
#include <stack>

namespace pprint {

class PrettyPrintBuilder {
	std::stack<PrettyPrintedWithChildren_p> containerStack;
	PrettyPrinted_p _recentItem;
	PrettyPrintedWithChildren_p _root;
public:
	PrettyPrintBuilder();
	~PrettyPrintBuilder();

	bool hasCurrentContainer();
	unsigned int depth();
	PrettyPrintedWithChildren_p currentContainer();
	PrettyPrinted_p recentItem();

	PrettyPrintBuilder& add(PrettyPrinted_p item);
	PrettyPrintBuilder& add(PrettyPrinted* item);

	PrettyPrintBuilder& add(std::string text);

	PrettyPrintBuilder& addAndSelect(PrettyPrintedWithChildren_p container);
	PrettyPrintBuilder& addAndSelect(PrettyPrintedWithChildren* container);

	PrettyPrintBuilder& columns();
	PrettyPrintBuilder& append();

	PrettyPrintBuilder& up();

	PrettyPrinted_p build();
};

} // end of namespace pprint

#endif // not defined __HEADER_PPRINT_BUILDER__
