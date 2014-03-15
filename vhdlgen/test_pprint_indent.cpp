#include <list>
#include <sstream>
#include "test_pprint.h"

using namespace pprint;
using namespace testing;

class PPrintIndentTest : public ::testing::Test, public PrettyPrintTestHelpers {
protected:
	PPrintIndentTest() { }

	void SetUp() {
	}

	boost::shared_ptr<Indent> build(std::string text,
			std::string prefix = "", std::string postfix = "") {
		return build(Text::create(text), prefix, postfix);
	}

	boost::shared_ptr<Indent> build(PrettyPrinted_p item,
			std::string prefix = "", std::string postfix = "") {
		boost::shared_ptr<Indent> subject(new Indent(prefix, postfix));
		subject->add(item);
		return subject;
	}
};

TEST_F(PPrintIndentTest, testEmpty) {
	EXPECT_LINES(build(Empty::instance(), "abc", "def"));
}

TEST_F(PPrintIndentTest, testSingleLine) {
	EXPECT_LINES(build("CHILD", "abc", "def"),
		"abcCHILDdef");
}

TEST_F(PPrintIndentTest, testMultiline) {
	EXPECT_LINES(build("A\nB\nCD\nE", "abc", "def"),
		"abcAdef",
		"abcBdef",
		"abcCDdef",
		"abcEdef");
}

TEST_F(PPrintIndentTest, testOnlyPrefix) {
	EXPECT_LINES(build("A\nB\nCD\nE", "abc"),
		"abcA",
		"abcB",
		"abcCD",
		"abcE");
}

TEST_F(PPrintIndentTest, testOnlySuffix) {
	EXPECT_LINES(build("A\nB\nCD\nE", "", "def"),
		"Adef",
		"Bdef",
		"CDdef",
		"Edef");
}

TEST_F(PPrintIndentTest, testDefaultPrefixAndSuffixIsEmpty) {
	boost::shared_ptr<Indent> subject(new Indent());
	subject->add(Text::create("abc"));
	
	EXPECT_OUTPUT("abc", stream, stream << *subject);
}

TEST_F(PPrintIndentTest, testEmptyLinesGetPrefixAndSuffix) {
	EXPECT_LINES(build("\n\nCD\nE\n", "abc", "def"),
		"abcdef",
		"abcdef",
		"abcCDdef",
		"abcEdef",
		"abcdef");
}

TEST_F(PPrintIndentTest, testIndentWithoutChildIsEmpty) {
	boost::shared_ptr<Indent> subject(new Indent());
	
	EXPECT_LINES(subject);
}

TEST_F(PPrintIndentTest, testMeasureDoesntFail) {
	boost::shared_ptr<Indent> subject(new Indent());
	subject->measure();

	subject.reset(new Indent());
	subject->add(Empty::instance());
	subject->measure();
}

TEST_F(PPrintIndentTest, testThreeArgumentConstructor) {
	boost::shared_ptr<Indent> subject(new Indent("a", Text::create("foo\nbar"), "c"));
	
	EXPECT_LINES(subject, "afooc", "abarc");
}
