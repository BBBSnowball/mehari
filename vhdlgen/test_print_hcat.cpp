#include "test_pprint.h"

using namespace pprint;

class PPrintHCatTest : public ::testing::Test {
protected:
	boost::scoped_ptr<HCat> empty_hcat, single_col_hcat, same_height_hcat, different_height_hcat,
		nested_hcat, with_empty_cols_hcat, only_empty_cols_hcat;
	const PrettyPrinted *empty, *single_col, *same_height, *different_height,
		*nested, *with_empty_cols, *only_empty_cols;

	// Test patterns
	// -------------
	//
	// single_col:
	// abc
	// de
	//
	// same_height:
	// abc|g |jklm
	// de |hi|op
	//
	// different_height:
	// abcde|g |jklm|JKLM
	//      |hi|op  |OP
	//      |xy
	//
	// nested:
	// a|abcde|g |jklm|JKLM|abc|g |jklm
	//  |     |hi|op  |OP  |de |hi|op
	//  |     |xy          '
	//
	// with_empty_cols:
	// g
	//
	// empty and only_empty_cols don't produce any output.

	void SetUp() {
		empty_hcat.reset(new HCat());
		empty_hcat->measure();

		single_col_hcat.reset(new HCat());
		single_col_hcat->add(Text::create("abc\nde"));
		single_col_hcat->measure();

		same_height_hcat.reset(new HCat());
		same_height_hcat->add(Text::create("abc\nde"));
		same_height_hcat->add(Text::create("g\nhi"));
		same_height_hcat->add(Text::create("jklm\nop"));
		same_height_hcat->measure();

		different_height_hcat.reset(new HCat());
		different_height_hcat->add(Text::create("abcde"));
		different_height_hcat->add(Text::create("g\nhi\nxy"));
		different_height_hcat->add(Text::create("jklm\nop"));
		different_height_hcat->add(Text::create("JKLM\nOP"));
		different_height_hcat->measure();

		nested_hcat.reset(new HCat());
		nested_hcat->add(new Text("a"));
		nested_hcat->add(new HCat(*different_height_hcat));
		nested_hcat->add(new HCat(*same_height_hcat));
		nested_hcat->measure();

		with_empty_cols_hcat.reset(new HCat());
		with_empty_cols_hcat->add(new Empty());
		with_empty_cols_hcat->add(new Text("g"));
		with_empty_cols_hcat->add(new Empty());
		with_empty_cols_hcat->measure();

		only_empty_cols_hcat.reset(new HCat());
		only_empty_cols_hcat->add(new Empty());
		only_empty_cols_hcat->add(new Empty());
		only_empty_cols_hcat->measure();

		empty            = empty_hcat.get();
		single_col       = single_col_hcat.get();
		same_height      = same_height_hcat.get();
		different_height = different_height_hcat.get();
		nested           = nested_hcat.get();
		with_empty_cols  = with_empty_cols_hcat.get();
		only_empty_cols  = only_empty_cols_hcat.get();
	}

	void TearDown() {
	}
};

TEST_F(PPrintHCatTest, testCopyConstructor) {
	HCat copy(*single_col_hcat);
	ASSERT_TRUE(copy._measured());
	EXPECT_EQ(3u, copy.width());
	EXPECT_EQ(2u, copy.height());
}

TEST_F(PPrintHCatTest, testWidth) {
	EXPECT_EQ(3u, single_col->width());
	EXPECT_EQ(9u, same_height->width());
}

TEST_F(PPrintHCatTest, testWidthWorksWithDifferentHeights) {
	EXPECT_EQ(15u, different_height->width());
}

TEST_F(PPrintHCatTest, testWidthWorksWithNestedItems) {
	EXPECT_EQ(25u, nested->width());
}

TEST_F(PPrintHCatTest, testWidthIgnoresEmptyItems) {
	EXPECT_EQ(1u, with_empty_cols->width());
}

TEST_F(PPrintHCatTest, testDefaultWidthIsZero) {
	EXPECT_EQ(0u, empty->width());
	EXPECT_EQ(0u, only_empty_cols->width());
}

TEST_F(PPrintHCatTest, testHeight) {
	EXPECT_EQ(2u, single_col->height());
	EXPECT_EQ(2u, same_height->height());
}

TEST_F(PPrintHCatTest, testHeightWorksWithColumnsWithDifferentHeights) {
	EXPECT_EQ(3u, different_height->height());
}

TEST_F(PPrintHCatTest, testHeightWorksWithNestedColumns) {
	EXPECT_EQ(3u, nested->height());
}

TEST_F(PPrintHCatTest, testHeightIgnoresEmptyItems) {
	EXPECT_EQ(1u, with_empty_cols->height());
}

TEST_F(PPrintHCatTest, testDefaultHeightIsZero) {
	EXPECT_EQ(0u, empty->height());
	EXPECT_EQ(0u, only_empty_cols->height());
}

TEST_F(PPrintHCatTest, testIteratorWorksForEmptyHCat) {
	boost::scoped_ptr<LineIterator> iter(empty->lines());
	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintHCatTest, testIteratorWorksForSingleColumn) {
	boost::scoped_ptr<LineIterator> iter(single_col->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("abc", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("de", iter);

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintHCatTest, testIteratorWorksForMultiColumnAndPaddingIsRight) {
	boost::scoped_ptr<LineIterator> iter(same_height->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("abcg jklm", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("de hiop", iter);

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintHCatTest, testIteratorWorksForMultiColumnWithDifferentHeight) {
	boost::scoped_ptr<LineIterator> iter(different_height->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("abcdeg jklmJKLM", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("     hiop  OP", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("     xy", iter);

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintHCatTest, testIteratorWorksForNestedHCat) {
	boost::scoped_ptr<LineIterator> iter(nested->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("aabcdeg jklmJKLMabcg jklm", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("      hiop  OP  de hiop", iter);

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("      xy", iter);

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintHCatTest, testIteratorSkipsEmptyItems) {
	boost::scoped_ptr<LineIterator> iter(with_empty_cols->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_ITER_LINE_EQ("g", iter);

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintHCatTest, testIteratorWithOnlyEmptyItemsIsEmpty) {
	boost::scoped_ptr<LineIterator> iter(only_empty_cols->lines());

	EXPECT_FALSE(iter->next());
}

TEST_F(PPrintHCatTest, testEmptyIteratorDoesntHaveALastLine) {
	boost::scoped_ptr<LineIterator> iter(empty->lines());
	EXPECT_FALSE(iter->last());
}

TEST_F(PPrintHCatTest, testLastWorksForSingleColumn) {
	boost::scoped_ptr<LineIterator> iter(single_col->lines());

	ASSERT_TRUE(iter->last());
	EXPECT_ITER_LINE_EQ("de", iter);
}

TEST_F(PPrintHCatTest, testLastWorksForMultiColumn) {
	boost::scoped_ptr<LineIterator> iter(same_height->lines());

	ASSERT_TRUE(iter->last());
	EXPECT_ITER_LINE_EQ("de hiop", iter);
}

TEST_F(PPrintHCatTest, testLastWorksForMultiColumnWithDifferentHeight) {
	boost::scoped_ptr<LineIterator> iter(different_height->lines());

	ASSERT_TRUE(iter->last());
	EXPECT_ITER_LINE_EQ("     xy", iter);
}

TEST_F(PPrintHCatTest, testLastWorksForNestedHCat) {
	boost::scoped_ptr<LineIterator> iter(nested->lines());

	ASSERT_TRUE(iter->last());
	EXPECT_ITER_LINE_EQ("      xy", iter);
}

TEST_F(PPrintHCatTest, testIteratorWithOnlyEmptyItemsDoesntHaveALastLine) {
	boost::scoped_ptr<LineIterator> iter(only_empty_cols->lines());

	EXPECT_FALSE(iter->last());
}

TEST_F(PPrintHCatTest, testPrinting) {
	EXPECT_OUTPUT("abc\nde", stream, stream << *single_col);

	EXPECT_OUTPUT("abcg jklm\nde hiop", stream, stream << *same_height);

	EXPECT_OUTPUT("abcdeg jklmJKLM\n     hiop  OP\n     xy",
		stream, stream << *different_height);

	EXPECT_OUTPUT("aabcdeg jklmJKLMabcg jklm\n      hiop  OP  de hiop\n      xy",
		stream, stream << *nested);

	EXPECT_OUTPUT("g", stream, stream << *with_empty_cols);

	EXPECT_OUTPUT("", stream, stream << *empty);

	EXPECT_OUTPUT("", stream, stream << *only_empty_cols);
}

TEST_F(PPrintHCatTest, testIsLast) {
	boost::scoped_ptr<LineIterator> iter(same_height->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_FALSE(iter->isLast());

	ASSERT_TRUE(iter->next());
	EXPECT_TRUE(iter->isLast());


	iter.reset(nested->lines());

	ASSERT_TRUE(iter->next());
	EXPECT_FALSE(iter->isLast());

	ASSERT_TRUE(iter->next());
	EXPECT_FALSE(iter->isLast());

	ASSERT_TRUE(iter->next());
	EXPECT_TRUE(iter->isLast());
}

TEST_F(PPrintHCatTest, isLastDoesntChangeIteratorState) {
	boost::scoped_ptr<LineIterator> iter(nested->lines());

	ASSERT_TRUE(iter->next());
	ASSERT_TRUE(iter->next());
	ASSERT_TRUE(iter->next());

	iter->isLast();

	EXPECT_TRUE(iter->isLast());
	EXPECT_EQ("      xy", iter->text());
}
