#include <assert.h>
#include "pprint.h"

namespace pprint {

Empty::Empty() { }

Empty::~Empty() { }

int Empty::width()  const { return 0; }
int Empty::height() const { return 0; }

class EmptyLineIterator : public LineIterator {
public:
	EmptyLineIterator() { }

	virtual ~EmptyLineIterator() { }

	virtual bool next() {
		return false;
	}

	virtual bool last() {
		return false;
	}

	virtual const std::string text() const {
		assert(false);
		return "";
	}

	virtual int width() const {
		assert(false);
		return 0;
	}

	virtual int print(std::ostream& stream, int width, PrettyPrintStatus& status) const {
		assert(false);
		return 0;
	}
};

LineIterator* Empty::lines() const {
	return new EmptyLineIterator();
}

boost::shared_ptr<Empty> Empty::_instance;

const boost::shared_ptr<Empty> Empty::instance() {
	if (!_instance)
		_instance.reset(new Empty());

	return _instance;
}

} // end of namespace pprint
