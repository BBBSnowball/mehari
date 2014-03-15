#include <assert.h>
#include <list>
#include <sstream>
#include <boost/scoped_ptr.hpp>
#include "pprint.h"

namespace pprint {

Indent::Indent(std::string prefix, std::string postfix)
	: _prefix(prefix), _postfix(postfix) { }

Indent::~Indent() { }

void Indent::measure() {
}

unsigned int Indent::width() const {
	if (!_child)
		return 0;

	unsigned int child_width = _child->width();
	if (child_width > 0 || _child->height() > 0)
		return child_width + _prefix.size() + _postfix.size();
	else
		// child doesn't have any lines, so we don't add the prefix or suffix
		return 0;
}

unsigned int Indent::height() const {
	if (_child)
		return _child->height();
	else
		return 0;
}

const PrettyPrinted_p Indent::child()   const { return _child;   }
const std::string&    Indent::prefix()  const { return _prefix;  }
const std::string&    Indent::postfix() const { return _postfix; }

class IndentLines : public LineIterator {
	const Indent& indent;
	boost::scoped_ptr<LineIterator> lines;
public:
	IndentLines(const Indent& indent) : indent(indent), lines(indent.child()->lines()) {
	}

	~IndentLines() {
	}

	virtual bool next() {
		return lines->next();
	}

	virtual bool last() {
		return lines->last();
	}

	virtual bool isLast() const {
		return lines->isLast();
	}

	virtual const std::string text() const {
		return indent.prefix() + lines->text() + indent.postfix();
	}

	virtual unsigned int width() const {
		return indent.prefix().size() + lines->width() + indent.postfix().size();
	}

	virtual unsigned int print(std::ostream& stream, unsigned int width,
			PrettyPrintStatus& status) const {
		unsigned int indent_width = indent.prefix().size() + indent.postfix().size();

		if (width > indent_width)
			width -= indent_width;
		else
			width = 0;

		stream << indent.prefix();
		unsigned int actual_width = indent_width + lines->print(stream, width - indent_width, status);
		stream << indent.postfix();

		return actual_width;
	}
};

LineIterator* Indent::lines() const {
	if (_child)
		return new IndentLines(*this);
	else
		return Empty::instance()->lines();
}

void Indent::_add(PrettyPrinted_p item) {
	assert(!_child);

	if (!_child)
		_child = item;
}

} // end of namespace pprint
