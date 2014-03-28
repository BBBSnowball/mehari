#include <stdio.h>

#include "gtest/gtest.h"
#include "gtest/tap.h"

void enableTap() {
// see https://github.com/kinow/gtest-tap-listener/blob/master/samples/src/gtest_main.cc
testing::TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();
listeners.Append(new tap::TapListener());
}

GTEST_API_ int main(int argc, char **argv) {
	// ::testing::InitGoogleMock(&argc, argv);
  testing::InitGoogleTest(&argc, argv);
  enableTap();
  return RUN_ALL_TESTS();
}
