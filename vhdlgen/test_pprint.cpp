#include <gtest/gtest.h>
#include <boost/scoped_ptr.hpp>
#include <sstream>
#include "pprint.h"

namespace pprint {

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

TEST(PPrintText, simpleMethods) {
	Text text("blub");

	EXPECT_EQ("blub", text.text());
	EXPECT_EQ(4, text.width());
	EXPECT_EQ(1, text.height());
}

TEST(PPrintText, testCopyConstructor) {
	Text text("blub");
	Text text2(text);

	EXPECT_EQ("blub", text2.text());
	EXPECT_EQ(4, text2.width());
	EXPECT_EQ(1, text2.height());
}

TEST(PPrintText, createReturnsEmptyLineForEmptyString) {
	// Empty has a height of 0, but Text("") has a height of 1.

	PrettyPrinted_p pprinted = Text::create("");

	Text* text = dynamic_cast<Text*>(pprinted.get());

	ASSERT_NOT_NULL(text);
	EXPECT_EQ("", text->text());
}

TEST(PPrintText, createReturnsTextObjectForSingleLines) {
	PrettyPrinted_p pprinted = Text::create("abc");

	Text* text = dynamic_cast<Text*>(pprinted.get());

	ASSERT_NOT_NULL(text);
	EXPECT_EQ("abc", text->text());
}

TEST(PPrintText, createReturnsVCatForMultipleLines) {
	PrettyPrinted_p pprinted = Text::create("abc\nblub");

	VCat* vcat = dynamic_cast<VCat*>(pprinted.get());

	ASSERT_NOT_NULL(vcat);
	ASSERT_EQ(2, vcat->size());

	Text* line1 = dynamic_cast<Text*>(vcat->at(0).get());
	Text* line2 = dynamic_cast<Text*>(vcat->at(1).get());
	ASSERT_NOT_NULL(line1);
	ASSERT_NOT_NULL(line2);

	EXPECT_EQ("abc",  line1->text());
	EXPECT_EQ("blub", line2->text());
}

TEST(PPrintText, createCanHandleASingleNewline) {
	PrettyPrinted_p pprinted = Text::create("\n");

	VCat* vcat = dynamic_cast<VCat*>(pprinted.get());

	ASSERT_NOT_NULL(vcat);
	ASSERT_EQ(2, vcat->size());

	Text* line1 = dynamic_cast<Text*>(vcat->at(0).get());
	Text* line2 = dynamic_cast<Text*>(vcat->at(1).get());
	ASSERT_NOT_NULL(line1);
	ASSERT_NOT_NULL(line2);

	EXPECT_EQ("", line1->text());
	EXPECT_EQ("", line2->text());
}

TEST(PPrintText, createCanHandleAConsecutiveNewline) {
	PrettyPrinted_p pprinted = Text::create("a\n\nb");

	VCat* vcat = dynamic_cast<VCat*>(pprinted.get());

	ASSERT_NOT_NULL(vcat);
	ASSERT_EQ(3, vcat->size());

	Text* line1 = dynamic_cast<Text*>(vcat->at(0).get());
	Text* line2 = dynamic_cast<Text*>(vcat->at(1).get());
	Text* line3 = dynamic_cast<Text*>(vcat->at(2).get());
	ASSERT_NOT_NULL(line1);
	ASSERT_NOT_NULL(line2);
	ASSERT_NOT_NULL(line3);

	EXPECT_EQ("a", line1->text());
	EXPECT_EQ("",  line2->text());
	EXPECT_EQ("b", line3->text());
}

class PPrintTextLinesIterator : public ::testing::Test {
protected:
	Text *empty, *blub;
	LineIterator *it_empty, *it_blub;

	void SetUp() {
		empty = new Text("");
		blub  = new Text("blub");

		it_empty = empty->lines();
		it_blub  = blub->lines();
	}

	void TearDown() {
		delete empty;
		delete blub;

		delete it_empty;
		delete it_blub;
	}
};

TEST_F(PPrintTextLinesIterator, hasExactlyOneLine) {
	ASSERT_TRUE(it_empty->next());
	ASSERT_FALSE(it_empty->next());

	ASSERT_TRUE(it_blub->next());
	ASSERT_FALSE(it_blub->next());
}

TEST_F(PPrintTextLinesIterator, nextContinueToReturnFalseAfterTheLastLine) {
	ASSERT_TRUE(it_blub->next());
	ASSERT_FALSE(it_blub->next());
	ASSERT_FALSE(it_blub->next());
	ASSERT_FALSE(it_blub->next());
}

TEST_F(PPrintTextLinesIterator, firstLineHasValidData) {
	ASSERT_TRUE(it_empty->next());
	ASSERT_TRUE(it_blub->next());

	EXPECT_EQ("",     it_empty->text());
	EXPECT_EQ("blub", it_blub->text());

	EXPECT_EQ(0, it_empty->width());
	EXPECT_EQ(4, it_blub->width());

	PrettyPrintStatus status;
	int result;
	EXPECT_OUTPUT("", stream, result = it_empty->print(stream, 0, status));
	EXPECT_EQ(0, result);
	EXPECT_OUTPUT("blub", stream, result = it_blub->print(stream, 0, status));
	EXPECT_EQ(4, result);
}

TEST_F(PPrintTextLinesIterator, lastReturnsTrue) {
	EXPECT_TRUE(it_empty->last());
	EXPECT_TRUE(it_blub->last());
}

TEST_F(PPrintTextLinesIterator, lastLineHasValidData) {
	ASSERT_TRUE(it_empty->last());
	ASSERT_TRUE(it_blub->last());

	EXPECT_EQ("",     it_empty->text());
	EXPECT_EQ("blub", it_blub->text());
}


class PPrintVCatTest : public ::testing::Test {
protected:
	boost::scoped_ptr<VCat> empty_vcat, single_line_vcat, multiline_vcat,
		nested_vcat, with_empty_items_vcat, only_empty_items_vcat;
	const PrettyPrinted *empty, *single_line, *multiline, *nested, *with_empty_items, *only_empty_items;

	void SetUp() {
		empty_vcat.reset(new VCat());
		empty_vcat->measure();

		single_line_vcat.reset(new VCat());
		single_line_vcat->add(new Text("blub"));
		single_line_vcat->measure();

		multiline_vcat.reset(new VCat());
		multiline_vcat->add(new Text("ab"));
		multiline_vcat->add(new Text("def"));
		multiline_vcat->add(new Text("g"));
		multiline_vcat->measure();

		nested_vcat.reset(new VCat());
		nested_vcat->add(new Text("a"));
		nested_vcat->add(new VCat(*single_line_vcat));
		nested_vcat->add(new VCat(*multiline_vcat));
		nested_vcat->measure();

		with_empty_items_vcat.reset(new VCat());
		with_empty_items_vcat->add(new Empty());
		with_empty_items_vcat->add(new Text("g"));
		with_empty_items_vcat->add(new Empty());
		with_empty_items_vcat->measure();

		only_empty_items_vcat.reset(new VCat());
		only_empty_items_vcat->add(new Empty());
		only_empty_items_vcat->add(new Empty());
		only_empty_items_vcat->measure();

		empty            = empty_vcat.get();
		single_line      = single_line_vcat.get();
		multiline        = multiline_vcat.get();
		nested           = nested_vcat.get();
		with_empty_items = with_empty_items_vcat.get();
		only_empty_items = only_empty_items_vcat.get();
	}

	void TearDown() {
	}
};

TEST_F(PPrintVCatTest, testCopyConstructor) {
	VCat vcat;
	vcat.add(new Text("abc"));
	vcat.measure();

	VCat vcat2(vcat);
	ASSERT_TRUE(vcat2.measured);
	EXPECT_EQ(3, vcat2.width());
	EXPECT_EQ(1, vcat2.height());
}

TEST_F(PPrintVCatTest, testWidth) {
	EXPECT_EQ(4, single_line->width());
	EXPECT_EQ(3, multiline->width());
}

TEST_F(PPrintVCatTest, testWidthWorksWithNestedItems) {
	EXPECT_EQ(4, nested->width());
}

TEST_F(PPrintVCatTest, testWidthIgnoresEmptyItems) {
	EXPECT_EQ(1, with_empty_items->width());
}

TEST_F(PPrintVCatTest, testDefaultWidthIsZero) {
	EXPECT_EQ(0, empty->width());
	EXPECT_EQ(0, only_empty_items->width());
}

TEST_F(PPrintVCatTest, testHeight) {
	EXPECT_EQ(1, single_line->height());
	EXPECT_EQ(3, multiline->height());
}

TEST_F(PPrintVCatTest, testHeightWorksWithNestedItems) {
	EXPECT_EQ(5, nested->height());
}

TEST_F(PPrintVCatTest, testHeightIgnoresEmptyItems) {
	EXPECT_EQ(1, with_empty_items->height());
}

TEST_F(PPrintVCatTest, testDefaultHeightIsZero) {
	EXPECT_EQ(0, empty->height());
	EXPECT_EQ(0, only_empty_items->height());
}

TEST_F(PPrintVCatTest, testIteratorWorksForEmptyVCat) {
	boost::scoped_ptr<LineIterator> iter(empty->lines());
	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintVCatTest, testIteratorWorksForNormalVCat) {
	boost::scoped_ptr<LineIterator> iter(single_line->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("blub", iter);

	EXPECT_FALSE(iter->next());


	iter.reset(multiline->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("ab", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("def", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("g", iter);

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintVCatTest, testIteratorWorksForNestedVCat) {
	boost::scoped_ptr<LineIterator> iter(nested->lines());

	ASSERT_TRUE(iter->next());
	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("blub", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("ab", iter);

	ASSERT_TRUE(iter->next());
	ASSERT_TRUE(iter->next());

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintVCatTest, testIteratorSkipsEmptyItems) {
	boost::scoped_ptr<LineIterator> iter(with_empty_items->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("g", iter);

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintVCatTest, testIteratorWithOnlyEmptyItemsIsEmpty) {
	boost::scoped_ptr<LineIterator> iter(only_empty_items->lines());

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintVCatTest, testEmptyIteratorDoesntHaveALastLine) {
	boost::scoped_ptr<LineIterator> iter(empty->lines());
	EXPECT_FALSE(iter->last());
}

TEST_F(PPrintVCatTest, testLastWorksForNormalVCat) {
	boost::scoped_ptr<LineIterator> iter(single_line->lines());

	ASSERT_TRUE(iter->last());
	EXPECT_ITER_LINE_EQ("blub", iter);


	iter.reset(multiline->lines());

	ASSERT_TRUE(iter->last());
	EXPECT_ITER_LINE_EQ("g", iter);
}

TEST_F(PPrintVCatTest, testLastWorksForNestedVCat) {
	boost::scoped_ptr<LineIterator> iter(nested->lines());

	ASSERT_TRUE(iter->last());
	EXPECT_ITER_LINE_EQ("g", iter);
}

TEST_F(PPrintVCatTest, testLastSkipsEmptyItems) {
	boost::scoped_ptr<LineIterator> iter(with_empty_items->lines());

	ASSERT_TRUE(iter->last());
	EXPECT_ITER_LINE_EQ("g", iter);
}

TEST_F(PPrintVCatTest, testIteratorWithOnlyEmptyItemsDoesntHaveALastLine) {
	boost::scoped_ptr<LineIterator> iter(only_empty_items->lines());

	EXPECT_FALSE(iter->last());
}

}
