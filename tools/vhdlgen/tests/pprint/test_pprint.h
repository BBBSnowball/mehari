#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/scoped_ptr.hpp>
#include <sstream>
#include <list>
#include "pprint.h"

#define ASSERT_NOT_NULL(x) ASSERT_TRUE(x) << #x << " must not be NULL."

#define EXPECT_OUTPUT(output, stream, code) { std::stringstream stream; code; EXPECT_EQ(output, stream.str()); }

#define EXPECT_ITER_LINE_EQ(expected, iter) {                                   \
	LineIterator* _iter = (iter).get();                                         \
	std::string _expected = (expected);                                         \
	EXPECT_EQ(_expected, _iter->text());                                        \
	EXPECT_EQ(_expected.size(), _iter->width());                                \
	PrettyPrintStatus status;                                                   \
	unsigned int result;                                                        \
	EXPECT_OUTPUT(_expected, stream, result = _iter->print(stream, 0, status)); \
	EXPECT_EQ(_expected.size(), result);                                        \
}

class PrettyPrintTestHelpers {
protected:
	typedef std::list<std::string>::iterator line_iter;
	void expect_lines(std::string file, unsigned int line, std::string pprinted_code,
			pprint::PrettyPrinted_p pprinted, const char** lines, size_t count);

	void expect_current_line(const std::string& expected_line, bool should_be_last_line,
			pprint::LineIterator* line_iter, std::string line_info);
};

#define EXPECT_LINES(pprinted, ...) {                                                      \
		const char* lines[] = { __VA_ARGS__ };                                                  \
		expect_lines(__FILE__, __LINE__, #pprinted, (pprinted), lines, sizeof(lines)/sizeof(*lines)); \
	}

namespace pprint {

class MockPrettyPrintable : public PrettyPrintable {
public:
	MOCK_CONST_METHOD0(prettyPrint, const PrettyPrinted_p());
};

class MockPrettyPrinted : public PrettyPrinted {
public:
	MOCK_CONST_METHOD0(lines, LineIterator*());

	MOCK_CONST_METHOD0(width,  unsigned int());
	MOCK_CONST_METHOD0(height, unsigned int());
};

class MockLineIterator : public LineIterator {
public:
	MOCK_METHOD0(next, bool());
	MOCK_METHOD0(last, bool());

	MOCK_CONST_METHOD0(isLast, bool());
	MOCK_CONST_METHOD0(text,  const std::string());
	MOCK_CONST_METHOD0(width, unsigned int());
	MOCK_CONST_METHOD3(print, unsigned int(std::ostream& stream, unsigned int width, PrettyPrintStatus& status));
};

}
