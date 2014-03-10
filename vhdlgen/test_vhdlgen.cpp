#include "test_vhdlgen.h"

using namespace vhdl;

class VHDLLibraryTest : public ::testing::Test {
protected:
	UsedLibraries libs;

	virtual void SetUp() {
	}
};

TEST(VHDLUsedLibrary, getNameReturnsTheName) {
	UsedLibrary lib("blub");
	EXPECT_EQ("blub", lib.getName());
}

TEST(VHDLUsedLibrary, libraryIsEmptyByDefault) {
	UsedLibrary lib("blub");
	EXPECT_TRUE(lib.empty());
}

TEST(VHDLUsedLibrary, elementsAreAppended) {
	UsedLibrary lib("blub");
	lib << "abc.all";
	lib << "def.all";

	EXPECT_EQ(2, lib.size());
	EXPECT_EQ("abc.all", lib.at(0));
	EXPECT_EQ("def.all", lib.at(1));
}

TEST(VHDLUsedLibrary, librarySupportsFluentSyntax) {
	UsedLibrary lib("blub");
	lib << "abc.all"
	    << "def.all";

	EXPECT_EQ(2, lib.size());
}

TEST(VHDLUsedLibraries, supportsFluentSyntax) {
	UsedLibraries libs;

	libs.add("blub")
		<< "abc.all"
		<< "def.all";

	EXPECT_EQ(2, libs["blub"].size());
}

TEST(VHDLUsedLibraries, pprint) {
	UsedLibraries libs;

	libs.add("blub")
		<< "abc.all"
		<< "def.all";

	libs.add("bar");

	libs.add("bla")
		<< "x.all";

	// Libraries are sorted, i.e. we will get "bar", "bla", "blub"

	EXPECT_PRETTY_PRINTED(libs,
		"library bar;\n"
		"\n"
		"library bla;\n"
		"use bla.x.all;\n"
		"\n"
		"library blub;\n"
		"use blub.abc.all;\n"
		"use blub.def.all;");
}
