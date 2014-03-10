#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/scoped_ptr.hpp>
#include <sstream>
#include "vhdlgen.h"

#define ASSERT_NOT_NULL(x) ASSERT_TRUE(x) << #x << " must not be NULL."

#define EXPECT_OUTPUT(output, stream, code) { std::stringstream stream; code; EXPECT_EQ(output, stream.str()); }

#define EXPECT_PRETTY_PRINTED(object, expected) EXPECT_OUTPUT(expected, stream, stream << (object).prettyPrint())
