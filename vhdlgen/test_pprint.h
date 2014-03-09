#include <gtest/gtest.h>
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
	int result;                                                                 \
	EXPECT_OUTPUT(_expected, stream, result = _iter->print(stream, 0, status)); \
	EXPECT_EQ(_expected.size(), result);                                        \
}
