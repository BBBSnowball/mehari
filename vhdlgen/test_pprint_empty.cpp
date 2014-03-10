#include "test_pprint.h"

using namespace pprint;

TEST(PPrintEmpty, simpleMethods) {
	Empty empty;

	EXPECT_EQ(0, empty.width());
	EXPECT_EQ(0, empty.height());
}

TEST(PPrintEmpty, lineIterator) {
	Empty empty;

	boost::scoped_ptr<LineIterator> iter(empty.lines());

	EXPECT_FALSE(iter->next());
	EXPECT_FALSE(iter->last());

	// We cannot use any other method because next() and last() always return false.
}

TEST(PPrintEmpty, testCopyConstructor) {
	Empty empty;
	Empty empty2(empty);
}

TEST(PPrintEmpty, testInstance) {
	PrettyPrinted_p empty1 = Empty::instance(),
	                empty2 = Empty::instance();

	EXPECT_EQ(empty1, empty2);
	EXPECT_TRUE(dynamic_cast<Empty*>(empty1.get()));
}
