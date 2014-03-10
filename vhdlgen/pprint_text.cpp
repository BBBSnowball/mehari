#include <assert.h>
#include "pprint.h"

namespace pprint {

Text::Text(std::string text) : _text(text) {
	assert(text.find('\n') == std::string::npos);
}

Text::~Text() { }

PrettyPrinted_p Text::create(std::string text) {
	size_t prev_pos = 0;
	size_t pos = text.find('\n', prev_pos);

	if (pos == std::string::npos)
		return PrettyPrinted_p(new Text(text));

	boost::shared_ptr<VCat> lines(new VCat());
	do {
		std::string line = text.substr(prev_pos, pos - prev_pos);
		lines->push_back(PrettyPrinted_p(new Text(line)));

		prev_pos = pos+1;
		pos = text.find('\n', prev_pos);
	} while (pos != std::string::npos);

	std::string last_line = text.substr(prev_pos);
	lines->push_back(PrettyPrinted_p(new Text(last_line)));

	lines->measure();

	return lines;
}

class SingleLineIterator : public LineIterator {
	std::string _text;
	int line;
public:
	SingleLineIterator(std::string text) : _text(text), line(-1) {
	}

	virtual ~SingleLineIterator() { }

	virtual bool next() {
		if (line == -1) {
			line = 0;
			return true;
		} else {
			line = 2;
			return false;
		}
	}

	virtual bool last() {
		assert(line <= 0);

		line = 0;
		return true;
	}

	virtual const std::string text() const {
		return _text;
	}

	virtual int width() const {
		return _text.size();
	}

	virtual int print(std::ostream& stream, int width, PrettyPrintStatus& status) const {
		stream << _text;
		return this->width();
	}
};

LineIterator* Text::lines() const {
	return new SingleLineIterator(_text);
}

const std::string Text::text()   const { return _text; }
int               Text::width()  const { return _text.size(); }
int               Text::height() const { return 1; }

} // end of namespace pprint
