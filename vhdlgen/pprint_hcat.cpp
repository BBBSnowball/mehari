#include <assert.h>
#include <list>
#include <sstream>
#include "pprint.h"

namespace pprint {

typedef std::vector<PrettyPrinted_p>::const_iterator iter_t;

HCat::HCat() { }

HCat::~HCat() { }

void HCat::measure() {
	_width  = 0;
	_height = 0;

	for (iter_t it = this->begin(); it != this->end(); ++it) {
		_width += (*it)->width();

		int height = (*it)->height();
		if (height > _height)
			_height = height;
	}

	measured = true;
}

int HCat::width() const {
	assert(measured);

	return _width;
}

int HCat::height() const {
	assert(measured);

	return _height;
}

class HCatLines : public LineIterator {
	typedef std::pair<LineIterator*, int> LinesAndWidth;
	std::list<LinesAndWidth> line_iters;
	typedef std::list<LinesAndWidth>::iterator ll_iter_t;
	typedef std::list<LinesAndWidth>::const_iterator const_ll_iter_t;

	int live_iterators;

	const HCat& hcat;

	bool valid() const { return live_iterators > 0; }

	void removeEmptyIteratorsAtTheEnd() {
		// We cannot use reverse_iterator with erase, so we use normal iterators. Fortunately,
		// they are bidirectional.
		for (ll_iter_t iter = line_iters.end(); iter != line_iters.begin(); ) {
			--iter;

			if (!iter->first) {
				iter = line_iters.erase(iter);
			} else
				break;
		}
	}
public:
	HCatLines(const HCat& hcat) : hcat(hcat) {
		live_iterators = 0;
		for (iter_t iter = hcat.begin(); iter != hcat.end(); ++iter) {
			line_iters.push_back(LinesAndWidth((*iter)->lines(), (*iter)->width()));
			live_iterators++;
		}
	}

	~HCatLines() {
		for (ll_iter_t iter = line_iters.begin(); iter != line_iters.end(); ++iter)
			if (iter->first)
				delete iter->first;
	}

	virtual bool next() {
		for (ll_iter_t iter = line_iters.begin(); iter != line_iters.end(); ++iter) {
			if (iter->first && !iter->first->next()) {
				delete iter->first;
				iter->first = NULL;
				live_iterators--;
			}
		}

		removeEmptyIteratorsAtTheEnd();

		return valid();
	}

	virtual bool last() {
		assert(hcat.size() == line_iters.size());

		if (hcat.size() != line_iters.size())
			// This is a serious error, so we cannot continue.
			return false;

		int height = hcat.height();

		iter_t iter;
		ll_iter_t ll_iter;
		for (iter = hcat.begin(), ll_iter = line_iters.begin();
				iter != hcat.end() && ll_iter != line_iters.end(); ++iter, ++ll_iter) {
			assert(ll_iter->first);
			if (!ll_iter->first)
				// This is an error.
				return false;

			if ((*iter)->height() != height || !ll_iter->first->last()) {
				delete ll_iter->first;
				ll_iter->first = NULL;
				live_iterators--;
			}
		}

		removeEmptyIteratorsAtTheEnd();

		return valid();
	}

	virtual const std::string text() const {
		assert(valid());

		PrettyPrintStatus status;
		std::stringstream stream;
		print(stream, 0, status);
		return stream.str();
	}

	virtual int width() const {
		assert(valid());

		int width = 0;
		for (const_ll_iter_t iter = line_iters.begin(); iter != line_iters.end(); ++iter) {
			if (iter != --line_iters.end())
				width += iter->second;
			else if (iter->first)
				// last column is not padded
				width += iter->first->width();
		}

		return width;
	}

	virtual int print(std::ostream& stream, int width, PrettyPrintStatus& status) const {
		assert(valid());

		int actual_width = 0;
		for (const_ll_iter_t iter = line_iters.begin(); iter != line_iters.end(); ++iter) {
			if (iter != --line_iters.end()) {
				int column_width = iter->second;
				int used_width = 0;
				if (iter->first)
					used_width = iter->first->print(stream, column_width, status);

				actual_width += column_width;

				if (used_width < column_width) {
					int remaining_space = column_width - used_width;

					for (int i=0; i<remaining_space; i++)
						stream << ' ';
				}
			} else if (iter->first) {
				// last column is not padded and it may use all the remaining width
				int column_width = iter->second;
				int remaining_width = width - actual_width;
				if (column_width < remaining_width)
					column_width = remaining_width;

				int used_width = iter->first->print(stream, column_width, status);

				actual_width += used_width;
			}
		}

		return actual_width;
	}
};

LineIterator* HCat::lines() const {
	return new HCatLines(*this);
}

} // end of namespace pprint
