#include "test_pprint.h"

// Tests must be in the same namespace as the tested classes to access private members via a friend declaration.
namespace pprint {

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
	ASSERT_TRUE(vcat2._measured());
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

TEST_F(PPrintVCatTest, testPrinting) {
	EXPECT_OUTPUT("", stream, stream << *empty);

	EXPECT_OUTPUT("blub", stream, stream << *single_line);

	EXPECT_OUTPUT("ab\ndef\ng", stream, stream << *multiline);

	EXPECT_OUTPUT("a\nblub\nab\ndef\ng", stream, stream << *nested);

	EXPECT_OUTPUT("g", stream, stream << *with_empty_items);

	EXPECT_OUTPUT("", stream, stream << *only_empty_items);
}

} // end of namespace pprint
