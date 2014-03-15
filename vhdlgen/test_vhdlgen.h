#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/scoped_ptr.hpp>
#include <sstream>
#include "vhdlgen.h"

#define ASSERT_NOT_NULL(x) ASSERT_TRUE(x) << #x << " must not be NULL."

#define EXPECT_OUTPUT(output, stream, code) { std::stringstream stream; code; EXPECT_EQ(output, stream.str()); }

#define EXPECT_PRETTY_PRINTED(object, expected) EXPECT_OUTPUT(expected, stream, stream << (object).prettyPrint())

#define EXPECT_IS_INSTANCE(pointer_type, object) EXPECT_TRUE(dynamic_cast<pointer_type>(object))

#define EXPECT_PRETTY_PRINTED_IS_EMPTY(object) EXPECT_IS_INSTANCE(pprint::Empty*, (object).prettyPrint().get())

#define EXPECT_HAS_NO_LINES(object) { \
		boost::scoped_ptr<pprint::LineIterator> lines((object).prettyPrint()->lines()); \
		EXPECT_FALSE(lines->next()); \
	}
