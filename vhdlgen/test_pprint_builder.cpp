#include "pprint_builder.h"
#include "test_pprint.h"

using namespace pprint;

TEST(PPrintBuilderTest, testDefaultValues) {
	PrettyPrintBuilder builder;

	EXPECT_FALSE(builder.hasCurrentContainer());
	EXPECT_EQ(0, builder.depth());
}

TEST(PPrintBuilderTest, testWithOneContainer) {
	PrettyPrintBuilder builder;
	VCat* container = new VCat();

	builder.addAndSelect(container);

	ASSERT_TRUE(         builder.hasCurrentContainer());
	EXPECT_EQ(1,         builder.depth());
	EXPECT_EQ(container, builder.currentContainer().get());
	EXPECT_EQ(container, builder.recentItem().get());
}

TEST(PPrintBuilderTest, testWithOneContainerAndOnePieceOfContent) {
	PrettyPrintBuilder builder;
	VCat* container = new VCat();
	Text* text = new Text("blub");

	builder.addAndSelect(container);
	builder.add(text);

	ASSERT_TRUE(         builder.hasCurrentContainer());
	EXPECT_EQ(1,         builder.depth());
	EXPECT_EQ(container, builder.currentContainer().get());
	EXPECT_EQ(text,      builder.recentItem().get());
}

TEST(PPrintBuilderTest, testWithTwoContainers) {
	PrettyPrintBuilder builder;
	VCat* container = new VCat();
	VCat* container2 = new VCat();

	builder.addAndSelect(container);
	builder.addAndSelect(container2);

	ASSERT_TRUE(          builder.hasCurrentContainer());
	ASSERT_EQ(2,          builder.depth());
	EXPECT_EQ(container2, builder.currentContainer().get());
	EXPECT_EQ(container2, builder.recentItem().get());
}

TEST(PPrintBuilderTest, testUp) {
	PrettyPrintBuilder builder;
	VCat* container = new VCat();
	VCat* container2 = new VCat();

	builder.addAndSelect(container);
	builder.addAndSelect(container2);

	ASSERT_EQ(2, builder.depth());

	builder.up();

	ASSERT_EQ(1, builder.depth());

	EXPECT_EQ(container,  builder.currentContainer().get());
	EXPECT_EQ(container2, builder.recentItem().get());
}

TEST(PPrintBuilderTest, testAddString) {
	PrettyPrintBuilder builder;
	EXPECT_OUTPUT("blub", stream, stream << builder.append().add("blub").build());
}

TEST(PPrintBuilderTest, testAppend) {
	PrettyPrintBuilder builder;
	EXPECT_OUTPUT("blub\nxyz", stream, stream << builder.append().add("blub").add("xyz").build());
}

TEST(PPrintBuilderTest, testColumns) {
	PrettyPrintBuilder builder;
	EXPECT_OUTPUT("blubxyz", stream, stream << builder.columns().add("blub").add("xyz").build());
}

TEST(PPrintBuilderTest, testTable) {
	PrettyPrintBuilder builder;
	EXPECT_OUTPUT("blubg\nxyz hijk", stream,
		stream << builder
			.columns()
				.append()
					.add("blub")
					.add("xyz")
					.up()
				.append()
					.add("g")
					.add("hijk")
					.up()
				.up()
			.build());
}

TEST(PPrintBuilderTest, testBuildAscendsToTopLevel) {
	PrettyPrintBuilder builder;
	EXPECT_OUTPUT("blubg\nxyz hijk", stream,
		stream << builder
			.columns()
				.append()
					.add("blub")
					.add("xyz")
					.up()
				.append()
					.add("g")
					.add("hijk")
			.build());

	EXPECT_FALSE(builder.hasCurrentContainer());
	EXPECT_EQ(0, builder.depth());
}

TEST(PPrintBuilderTest, testBuilderCanBeReused) {
	PrettyPrintBuilder builder;
	EXPECT_OUTPUT("foo", stream, stream << builder.append().add("foo").build());
	EXPECT_OUTPUT("bar", stream, stream << builder.append().add("bar" ).build());
}

TEST(PPrintBuilderTest, testAddAutomaticallyAddsVCatAsRoot) {
	PrettyPrintBuilder builder;

	EXPECT_FALSE(builder.hasCurrentContainer());

	builder.add("abc").add("def");

	EXPECT_TRUE(builder.hasCurrentContainer());
	EXPECT_EQ(1, builder.depth());

	EXPECT_OUTPUT("abc\ndef", stream, stream << builder.build());
}

TEST(PPrintBuilderTest, testSeperateBy) {
	PrettyPrintBuilder builder;

	builder
		.append()
			.seperateBy("X")
			.add("abc")
			.add("def");

	EXPECT_OUTPUT("abc\nX\ndef", stream, stream << builder.build());
}

TEST(PPrintBuilderTest, testSeperateByCreatesDefaultContainer) {
	PrettyPrintBuilder builder;

	builder
		.seperateBy("X")
		.add("abc")
		.add("def");

	EXPECT_OUTPUT("abc\nX\ndef", stream, stream << builder.build());
}

TEST(PPrintBuilderTest, testSeperateByOnlyAffectsTheCurrentContainer) {
	PrettyPrintBuilder builder;

	builder
		.append()
			.columns()
				.seperateBy("X")
				.add("abc")
				.add("def")
				.up()
			.columns()
				.add("ghi")
				.add("jkl");

	EXPECT_OUTPUT("abcXdef\nghijkl", stream, stream << builder.build());
}

TEST(PPrintBuilderTest, testSeperateByDoesntAffectExistingItems) {
	PrettyPrintBuilder builder;

	builder
		.columns()
			.add("abc")
			.add("def")
			.seperateBy("X")
			.add("ghi")
			.add("jkl");

	EXPECT_OUTPUT("abcdefXghiXjkl", stream, stream << builder.build());
}

TEST(PPrintBuilderTest, testSeperateByIgnoresEmptyItems) {
	PrettyPrintBuilder builder;

	builder
		.columns()
			.seperateBy("X")
			.add("abc")
			.add(new Empty())
			.add("jkl");

	EXPECT_OUTPUT("abcXjkl", stream, stream << builder.build());
}
