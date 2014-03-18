#include <list>
#include <sstream>
#include "test_pprint.h"

using namespace pprint;

class PPrintVCatOverlappingTest : public ::testing::Test, public PrettyPrintTestHelpers {
protected:
	boost::shared_ptr<VCatOverlapping> subject;

	PPrintVCatOverlappingTest() : subject(new VCatOverlapping()) { }

	void add(PrettyPrinted_p item) {
		subject->add(item);
	}

	void addEmpty() {
		add(Empty::instance());
	}

	void add(std::string text) {
		add(Text::create(text));
	}

	void build() {
		subject->measure();
	}
};

#define EXPECT_LINES(pprinted, ...) {                                                      \
		const char* lines[] = { __VA_ARGS__ };                                                  \
		expect_lines(__FILE__, __LINE__, #pprinted, (pprinted), lines, sizeof(lines)/sizeof(*lines)); \
	}

TEST_F(PPrintVCatOverlappingTest, testEmpty) {
	build();

	EXPECT_LINES(subject);
}

TEST_F(PPrintVCatOverlappingTest, testEmptyItems) {
	addEmpty();
	addEmpty();
	build();

	EXPECT_LINES(subject);
}

TEST_F(PPrintVCatOverlappingTest, testSingleLine) {
	add("blub");
	build();

	EXPECT_LINES(subject, "blub");
}

TEST_F(PPrintVCatOverlappingTest, testSingleItem) {
	add("blub\nbla");
	build();

	EXPECT_LINES(subject, "blub", "bla");
}

TEST_F(PPrintVCatOverlappingTest, testEmptyItemsAtBeginningAreIgnored) {
	addEmpty();
	addEmpty();
	add("blub\nbla");
	build();

	EXPECT_LINES(subject, "blub", "bla");
}

TEST_F(PPrintVCatOverlappingTest, testEmptyItemsAtEndAreIgnored) {
	add("blub\nbla");
	addEmpty();
	addEmpty();
	build();

	EXPECT_LINES(subject, "blub", "bla");
}

TEST_F(PPrintVCatOverlappingTest, testEmptyItemsAtBeginningAreIgnoredBeforeSingleLine) {
	addEmpty();
	addEmpty();
	add("blub");
	build();

	EXPECT_LINES(subject, "blub");
}

TEST_F(PPrintVCatOverlappingTest, testEmptyItemsAtEndAreIgnoredAfterSingleLine) {
	add("bla");
	addEmpty();
	addEmpty();
	build();

	EXPECT_LINES(subject, "bla");
}

TEST_F(PPrintVCatOverlappingTest, testEmptyItemsInTheMiddleAreIgnored) {
	add("blub\nbla");
	addEmpty();
	addEmpty();
	add("foo\nbar");
	build();

	EXPECT_LINES(subject, "blub", "blafoo", "bar");
}

TEST_F(PPrintVCatOverlappingTest, testSingleLineAtTheEnd) {
	add("blub\nbla");
	addEmpty();
	addEmpty();
	add("foo");
	build();

	EXPECT_LINES(subject, "blub", "blafoo");
}

TEST_F(PPrintVCatOverlappingTest, testSingleLineAtBeginning) {
	add("foo");
	addEmpty();
	addEmpty();
	add("blub\nbla");
	build();

	EXPECT_LINES(subject, "fooblub", "bla");
}

TEST_F(PPrintVCatOverlappingTest, testOnlySingleLinesAndEmpty) {
	add("foo");
	addEmpty();
	addEmpty();
	add("blub");
	add("bla");
	build();

	EXPECT_LINES(subject, "fooblubbla");
}

TEST_F(PPrintVCatOverlappingTest, testTwoSingleAndADouble) {
	// this needs both pop_front calls in next() in the same call to next()
	//TODO well, no, it doesn't -> remove?
	add("foo");
	add("blub");
	add("foo\nbar");
	build();

	EXPECT_LINES(subject, "fooblubfoo", "bar");
}
