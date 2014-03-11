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

TEST(VHDLUsedLibrary, elementsAreSorted) {
	UsedLibrary lib("blub");
	lib << "abc.all";
	lib << "ghi.all";
	lib << "def.all";

	ASSERT_EQ(3, lib.size());
	UsedLibrary::iterator iter = lib.begin();
	EXPECT_EQ("abc.all", *    iter);
	EXPECT_EQ("def.all", * ++ iter);
	EXPECT_EQ("ghi.all", * ++ iter);
}

TEST(VHDLUsedLibrary, duplicatesAreIgnored) {
	UsedLibrary lib("blub");
	lib << "abc.all";
	lib << "def.all";
	lib << "def.all";
	lib << "abc.all";
	lib << "ghi.all";

	ASSERT_EQ(3, lib.size());
	UsedLibrary::iterator iter = lib.begin();
	EXPECT_EQ("abc.all", *    iter);
	EXPECT_EQ("def.all", * ++ iter);
	EXPECT_EQ("ghi.all", * ++ iter);
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

	// Libraries will be sorted by name, i.e. we will get "bar", "bla", "blub"

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

TEST(VHDLDirectionTest, instances) {
	EXPECT_EQ(Direction::Default.direction(), Direction_Default);
	EXPECT_EQ(Direction::In.direction(),      Direction_In);
	EXPECT_EQ(Direction::Out.direction(),     Direction_Out);
	EXPECT_EQ(Direction::InOut.direction(),   Direction_InOut);

	EXPECT_EQ(Direction::Default.str(), "Default");
	EXPECT_EQ(Direction::In.str(),      "In");
	EXPECT_EQ(Direction::Out.str(),     "Out");
	EXPECT_EQ(Direction::InOut.str(),   "InOut");
}

TEST(VHDLDirectionTest, pprint) {
	EXPECT_PRETTY_PRINTED(Direction::Default, "");
	EXPECT_PRETTY_PRINTED(Direction::In,      "in");
	EXPECT_PRETTY_PRINTED(Direction::Out,     "out");
	EXPECT_PRETTY_PRINTED(Direction::InOut,   "inout");
}

TEST(VHDLDirectionTest, pprintOfDefaultIsEmpty) {
	EXPECT_IS_INSTANCE(pprint::Empty*, Direction::Default.prettyPrint().get());
}

TEST(VHDLDirectionTest, isCopyConstructable) {
	Direction dir(Direction::Out);
	EXPECT_EQ(dir.direction(), Direction_Out);
	EXPECT_EQ(dir.str(),       "Out");
}

TEST(VHDLDirectionTest, hasDefaultConstructorAndAssignment) {
	Direction dir = Direction::Out;
	EXPECT_EQ(dir.direction(), Direction_Out);
	EXPECT_EQ(dir.str(),       "Out");
}

TEST(VHDLDirectionTest, equal) {
	Direction dir = Direction::Out;
	EXPECT_TRUE (Direction::Out == dir);
	EXPECT_FALSE(Direction::In  == dir);
	EXPECT_FALSE(Direction::Out != dir);
	EXPECT_TRUE (Direction::In  != dir);
}


TEST(VHDLType, getters) {
	Type type("std_logic_vector(42 downto 17)");

	EXPECT_EQ("std_logic_vector(42 downto 17)", type.type());
}

TEST(VHDLType, pprint) {
	Type type("std_logic_vector(42 downto 17)");

	EXPECT_PRETTY_PRINTED(type, "std_logic_vector(42 downto 17)");
}


class MyValueDeclaration : public ValueDeclaration {
public:
	MyValueDeclaration(std::string name, Direction direction, Type type)
		: ValueDeclaration(name, direction, type) { }
};

TEST(VHDLValueDeclaration, getters) {
	MyValueDeclaration vdecl("blub", Direction::In, Type("std_logic_vector(42 downto 17)"));

	EXPECT_EQ("blub",                           vdecl.name());
	EXPECT_EQ(Direction::In,                    vdecl.direction());
	EXPECT_EQ("std_logic_vector(42 downto 17)", vdecl.type().type());
}

TEST(VHDLValueDeclaration, pprint) {
	MyValueDeclaration vdecl1("blub", Direction::In,      Type("std_logic_vector(42 downto 17)"));
	MyValueDeclaration vdecl2("blub", Direction::Default, Type("std_logic_vector(42 downto 17)"));

	EXPECT_PRETTY_PRINTED(vdecl1, "blub : in std_logic_vector(42 downto 17)");
	EXPECT_PRETTY_PRINTED(vdecl2, "blub : std_logic_vector(42 downto 17)");
}


class MyValueDeclarationWithOptionalDefault : public ValueDeclarationWithOptionalDefault {
public:
	MyValueDeclarationWithOptionalDefault(std::string name, Direction direction, Type type)
		: ValueDeclarationWithOptionalDefault(name, direction, type) { }
	MyValueDeclarationWithOptionalDefault(std::string name, Direction direction, Type type, Value value)
		: ValueDeclarationWithOptionalDefault(name, direction, type, value) { }
};

TEST(VHDLValueDeclarationWithOptionalDefault, getters) {
	MyValueDeclarationWithOptionalDefault
		vdecl1("blub", Direction::In, Type("std_logic_vector(42 downto 17)")),
		vdecl2("blub", Direction::In, Type("std_logic_vector(42 downto 17)"), Value("(others => '0')"));

	EXPECT_EQ("blub",                           vdecl1.name());
	EXPECT_EQ(Direction::In,                    vdecl1.direction());
	EXPECT_EQ("std_logic_vector(42 downto 17)", vdecl1.type().type());
	EXPECT_FALSE(vdecl1.hasValue());

	EXPECT_EQ("blub",                           vdecl2.name());
	EXPECT_EQ(Direction::In,                    vdecl2.direction());
	EXPECT_EQ("std_logic_vector(42 downto 17)", vdecl2.type().type());
	ASSERT_TRUE(vdecl2.hasValue());
	EXPECT_EQ("(others => '0')",                vdecl2.value().value());
}

TEST(VHDLValueDeclarationWithOptionalDefault, pprint) {
	MyValueDeclarationWithOptionalDefault
		vdecl1("blub", Direction::In, Type("std_logic_vector(42 downto 17)")),
		vdecl2("blub", Direction::In, Type("std_logic_vector(42 downto 17)"), Value("(others => '0')"));

	EXPECT_PRETTY_PRINTED(vdecl1, "blub : in std_logic_vector(42 downto 17)");
	EXPECT_PRETTY_PRINTED(vdecl2, "blub : in std_logic_vector(42 downto 17) := (others => '0')");
}


Port makePort() {
	Port port;

	port.add(Pin("foo",  Direction::Out, Type("std_logic")));
	port.add(Pin("blub", Direction::In,  Type("std_logic")));

	return port;
}

TEST(VHDLPort, getters) {
	Port port(makePort());

	EXPECT_TRUE(port.contains("blub"));
	EXPECT_FALSE(port.contains("bar"));

	EXPECT_EQ("foo", port["foo"].name());

	EXPECT_TRUE(port.pinsByName().find("foo") != port.pinsByName().end());
	EXPECT_EQ(&port.pinsByName(), &port.pinsByName());

	ASSERT_EQ(2, port.pins().size());
	EXPECT_EQ("foo", port.pins().at(0)->name());
}

TEST(VHDLPort, pprint) {
	Port emptyPort;
	Port port(makePort());

	EXPECT_PRETTY_PRINTED(emptyPort, "port( );");
	EXPECT_PRETTY_PRINTED(port, "port(\n    foo : out std_logic;\n    blub : in std_logic\n);");
}
