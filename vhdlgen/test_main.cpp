#include <gtest/gtest.h>
#include <gtest/tap.h>

void enableTap() {
	// see https://github.com/kinow/gtest-tap-listener/blob/master/samples/src/gtest_main.cc
	testing::TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();
	//delete listeners.Release(listeners.default_result_printer());
	listeners.Append(new tap::TapListener());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	enableTap();
	return RUN_ALL_TESTS();
}
