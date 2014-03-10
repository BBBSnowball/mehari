#include "test_pprint.h"

using namespace pprint;

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

TEST(PPrintText, createCallsMeasure) {
	PrettyPrinted_p pprinted = Text::create("abc\nblub");

	VCat* vcat = dynamic_cast<VCat*>(pprinted.get());

	ASSERT_NOT_NULL(vcat);
	ASSERT_TRUE(vcat->_measured());
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

TEST_F(PPrintTextLinesIterator, validLineIsAlwaysTheLastOne) {
	ASSERT_TRUE(it_blub->next());

	EXPECT_TRUE(it_blub->isLast());
}

TEST_F(PPrintTextLinesIterator, isLastDoesntChangeIteratorState) {
	ASSERT_TRUE(it_blub->next());

	it_blub->isLast();

	EXPECT_TRUE(it_blub->isLast());
	EXPECT_EQ("blub", it_blub->text());
}
