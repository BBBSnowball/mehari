#include <gtest/gtest.h>
#include "pprint.h"

using namespace pprint;

#define ASSERT_NOT_NULL(x) ASSERT_TRUE(x)

TEST(PPrintText, simpleMethods) {
	Text text("blub");

	EXPECT_EQ("blub", text.text());
	EXPECT_EQ(4, text.width());
	EXPECT_EQ(1, text.height());
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


//	static boost::shared_ptr<PrettyPrinted> create(std::string text);

//	virtual LineIterator* lines() const;
