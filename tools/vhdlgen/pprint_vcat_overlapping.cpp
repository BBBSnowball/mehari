
#include <assert.h>
#include <boost/scoped_ptr.hpp>
#include <list>
#include <sstream>
#include "pprint.h"

namespace pprint {

typedef std::vector<PrettyPrinted_p>::const_iterator iter_t;

VCatOverlapping::VCatOverlapping() {
	measured = false;
}

VCatOverlapping::~VCatOverlapping() { }

void VCatOverlapping::measure() {
	_width  = 0;
	_height = 0;

	unsigned int prev_last_line_width = 0;
	for (iter_t it = this->begin(); it != this->end(); ++it) {
		unsigned int width = (*it)->width();
		if (width > _width)
			_width = width;

		unsigned int height = (*it)->height();
		if (_height == 0)
			// This is the first (probably) non-empty item.
			_height = height;
		else if (height > 1)
			// First line overlaps the previous item, so it doesn't increase the height.
			_height += height-1;

		// consider overlapping lines for width

		boost::scoped_ptr<LineIterator> lines((*it)->lines());
		if (lines->next()) {
			if (lines->isLast()) {
				// This is also the last line, so it may overlap with the next item.
				prev_last_line_width += lines->width();
			} else {
				unsigned int first_line_width = lines->width();
				lines.reset((*it)->lines());
				unsigned int last_line_width = lines->last() ? lines->width() : 0;

				if (prev_last_line_width + first_line_width > width)
					_width = prev_last_line_width + first_line_width;

				prev_last_line_width = last_line_width;
			}
		} else {
			// an empty item -> preserve value of prev_last_line_width
			// because the next item will overlap that line
		}
	}

	// prev_last_line_width can be greater than width, if the last item only
	// has a single line.
	if (prev_last_line_width > _width)
		_width = prev_last_line_width;

	measured = true;
}

unsigned int VCatOverlapping::width()  const {
	assert(measured);

	return _width;
}

unsigned int VCatOverlapping::height() const {
	assert(measured);

	return _height;
}

class VCatOverlappingLines : public LineIterator {
	iter_t begin, current, end;
	std::list<boost::shared_ptr<LineIterator> > current_lines;
	bool valid;
public:
	VCatOverlappingLines(iter_t begin, iter_t end) : begin(begin), current(begin), end(end) {
		this->valid = false;
	}

	~VCatOverlappingLines() {
	}

	virtual bool next() {
		// remove all but the last iterator from current_lines
		// because only the last iterator may have more lines
		while (current_lines.size() > 1)
			current_lines.pop_front();

		if (!current_lines.empty()) {
			if (!current_lines.front()->next()) {
				current_lines.pop_front();
			}
		}

		while (current_lines.empty() || current_lines.back()->isLast()) {
			// either there isn't any line or it is the last one of that item
			// -> add another line (if we can)
			
			if (current == end)
				// no more items
				break;

			boost::shared_ptr<LineIterator> lines((*current)->lines());
			if (lines->next()) {
				current_lines.push_back(lines);
			}

			++current;
		}

		valid = !current_lines.empty();
		return valid;
	}

	virtual bool last() {
		current_lines.clear();

		current = end;
		while (current != begin) {
			--current;

			boost::shared_ptr<LineIterator> lines((*current)->lines());
			bool more_than_one_line = lines->next() && !lines->isLast();
			lines.reset((*current)->lines());
			if (lines->last()) {
				current_lines.push_front(lines);
			}
			if (more_than_one_line)
				// We have a line break, so this is the last overlapping part.
				break;
		}

		// Make sure itLast() will return true
		current = end;

		valid = !current_lines.empty();
		return valid;
	}

	virtual bool isLast() const {
		assert(valid);

		// We are only interested in the last iterator because
		// only that one may have more lines.
		LineIterator* lines = !current_lines.empty() ? current_lines.back().get() : NULL;

		if (lines && !lines->isLast()) {
			// There is definetly at least one more line.
			return false;
		}

		iter_t tmp = current;
		if (tmp != end)
			++tmp;
		while (tmp != end) {
			boost::scoped_ptr<LineIterator> lines2((*tmp)->lines());

			if (lines2->next()) {
				// That one has at least one more line, so the current line is not the last one.
				return false;
			}

			++tmp;
		}

		return true;
	}

	virtual const std::string text() const {
		assert(valid);

		PrettyPrintStatus status;
		std::stringstream stream;
		print(stream, 0, status);
		return stream.str();
	}

	typedef std::list<boost::shared_ptr<LineIterator> >::const_iterator const_ll_iter_t;

	virtual unsigned int width() const {
		assert(valid);

		unsigned int width = 0;
		for (const_ll_iter_t iter = current_lines.begin(); iter != current_lines.end(); ++iter) {
			width += (*iter)->width();
		}

		return width;
	}

	virtual unsigned int print(std::ostream& stream, unsigned int width, PrettyPrintStatus& status) const {
		assert(valid);

		unsigned int actual_width = 0;
		for (const_ll_iter_t iter = current_lines.begin(); iter != current_lines.end(); ++iter) {
			unsigned int column_width;
			if (iter != --current_lines.end())
				// no padding
				column_width = 0;
			else
				// use remaining width
				column_width = width - actual_width;

			actual_width += (*iter)->print(stream, column_width, status);
		}

		return actual_width;
	}
};

LineIterator* VCatOverlapping::lines() const {
	return new VCatOverlappingLines(this->begin(), this->end());
}

void VCatOverlapping::_add(PrettyPrinted_p item) {
	this->push_back(item);
}

} // end of namespace pprint
