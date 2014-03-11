#include <list>
#include <sstream>
#include "test_pprint.h"

using namespace pprint;

class PPrintVCatOverlappingTest : public ::testing::Test {
protected:
	typedef std::list<std::string>::iterator line_iter;
	void expect_lines(std::string file, unsigned int line, std::string pprinted_code,
			PrettyPrinted_p pprinted, const char** lines, size_t count) {
		std::stringstream info_stream;
		info_stream << "called from " << file << ":" << line << " with " << pprinted_code;
		std::string info = info_stream.str();

		std::list<std::string> lines2;
		for (unsigned int i=0; i<count; i++)
			lines2.push_back(std::string(lines[i]));

		line_iter iter;
		unsigned int expected_width = 0;
		for (iter = lines2.begin(); iter != lines2.end(); ++iter) {
			unsigned int width = iter->size();
			if (width > expected_width)
				expected_width = width;
		}
		unsigned int expected_height = lines2.size();

		unsigned int line_number = 1;
		boost::scoped_ptr<LineIterator> line_iter(pprinted->lines());
		for (iter = lines2.begin(); iter != lines2.end(); ++iter) {
			std::stringstream line_info_stream;
			line_info_stream << "in line " << line_number << std::endl << info;
			std::string line_info = line_info_stream.str();

			bool has_more_lines = line_iter->next();

			EXPECT_TRUE(has_more_lines) << line_info;
			if (!has_more_lines)
				break;

			bool should_be_last_line = (iter == --lines2.end());
			expect_current_line(*iter, should_be_last_line, line_iter.get(), line_info);

			++line_number;
		}

		line_iter.reset(pprinted->lines());
		bool has_last_line = line_iter->last();
		EXPECT_EQ(!lines2.empty(), has_last_line);

		if (has_last_line && !lines2.empty()) {
			std::stringstream line_info_stream;
			line_info_stream << "in last line" << std::endl << info;
			std::string line_info = line_info_stream.str();

			bool should_be_last_line = true;
			expect_current_line(lines2.back(), should_be_last_line, line_iter.get(), line_info);
		}

		EXPECT_EQ(expected_width,  pprinted->width ()) << info;
		EXPECT_EQ(expected_height, pprinted->height()) << info;
	}

	void expect_current_line(const std::string& expected_line, bool should_be_last_line,
			LineIterator* line_iter, std::string line_info) {
		EXPECT_EQ(should_be_last_line, line_iter->isLast()) << line_info;

		EXPECT_EQ(expected_line, line_iter->text()) << line_info;

		EXPECT_EQ(expected_line.size(), line_iter->width()) << line_info;

		PrettyPrintStatus status;
		std::stringstream stream;
		unsigned int width_returned_by_print = line_iter->print(stream, 0, status);
		EXPECT_EQ(expected_line, stream.str()) << line_info;
		EXPECT_EQ(expected_line.size(), width_returned_by_print) << line_info;
	}


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
