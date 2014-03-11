#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/scoped_ptr.hpp>
#include <sstream>
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
