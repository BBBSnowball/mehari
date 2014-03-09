#include <assert.h>
#include "pprint.h"

namespace pprint {

std::ostream& operator <<(std::ostream& stream, const PrettyPrintable& data) {
	return stream << data.prettyPrint();
}


PrettyPrintStatus::PrettyPrintStatus()  { }
PrettyPrintStatus::~PrettyPrintStatus() { }


PrettyPrinted::~PrettyPrinted() { }


LineIterator::~LineIterator() { }


std::ostream& operator <<(std::ostream& stream, const boost::shared_ptr<PrettyPrinted> data) {
	return stream << *data;
}

std::ostream& operator <<(std::ostream& stream, const PrettyPrinted& data) {
	PrettyPrintStatus status;
	LineIterator* iter = data.lines();
	bool first = true;
	while (iter->next()) {
		if (first)
			first = false;
		else
			stream << std::endl;

		iter->print(stream, 0, status);
	}
}

} // end of namespace pprint
