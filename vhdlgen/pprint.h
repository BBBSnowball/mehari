#ifndef __HEADER_PPRINT__
#define __HEADER_PPRINT__

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

namespace pprint {

class PrettyPrinted;

typedef boost::shared_ptr<PrettyPrinted> PrettyPrinted_p;

class PrettyPrintable {
public:
	virtual const PrettyPrinted_p prettyPrint() const =0;
};

std::ostream& operator <<(std::ostream& stream, const PrettyPrintable& data);


class PrettyPrintStatus {
public:
	PrettyPrintStatus();
	~PrettyPrintStatus();
};

class LineIterator {
public:
	virtual ~LineIterator();

	virtual bool next() =0;
	virtual bool last() =0;

	virtual const std::string text() const =0;
	virtual int width() const =0;

	// print current line without newline, return width
	virtual int print(std::ostream& stream, int width, PrettyPrintStatus& status) const =0;
};

class PrettyPrinted {
public:
	virtual ~PrettyPrinted();

	virtual LineIterator* lines() const =0;

	virtual int width()  const =0;
	virtual int height() const =0;
};

std::ostream& operator <<(std::ostream& stream, const PrettyPrinted&  data);
std::ostream& operator <<(std::ostream& stream, const PrettyPrinted_p data);

class Text : public PrettyPrinted {
	std::string _text;
public:
	// use Text::create unless you are sure that the string doesn't have any newlines
	Text(std::string text);
	virtual ~Text();

	static PrettyPrinted_p create(std::string text);

	const std::string text() const;

	virtual LineIterator* lines() const;

	virtual int width()  const;
	virtual int height() const;
};

class Keyword : public Text { };

class VCat : public PrettyPrinted, public std::vector<PrettyPrinted_p> {
	bool measured;
	int _width;
	int _height;
public:
	VCat();
	virtual ~VCat();

	void measure();

	virtual LineIterator* lines() const;

	virtual int width()  const;
	virtual int height() const;
};

class HCat : public PrettyPrinted, public std::vector<PrettyPrinted_p> {
public:
	HCat();
	virtual ~HCat();

	void measure();

	virtual LineIterator* lines() const;

	virtual int width()  const;
	virtual int height() const;
};

} // end of namespace pprint

#endif // not defined __HEADER_PPRINT__
