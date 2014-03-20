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

	ASSERT_EQ(3u, lib.size());
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

	ASSERT_EQ(3u, lib.size());
	UsedLibrary::iterator iter = lib.begin();
	EXPECT_EQ("abc.all", *    iter);
	EXPECT_EQ("def.all", * ++ iter);
	EXPECT_EQ("ghi.all", * ++ iter);
}

TEST(VHDLUsedLibrary, librarySupportsFluentSyntax) {
	UsedLibrary lib("blub");
	lib << "abc.all"
	    << "def.all";

	EXPECT_EQ(2u, lib.size());
}

TEST(VHDLUsedLibraries, supportsFluentSyntax) {
	UsedLibraries libs;

	libs.add("blub")
		<< "abc.all"
		<< "def.all";

	EXPECT_EQ(2u, libs["blub"].size());
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
	EXPECT_PRETTY_PRINTED_IS_EMPTY(Direction::Default);
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
protected:
	virtual std::string prefix() const {
		return "my";
	}
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

	EXPECT_PRETTY_PRINTED(vdecl1, "my blub : in std_logic_vector(42 downto 17)");
	EXPECT_PRETTY_PRINTED(vdecl2, "my blub : in std_logic_vector(42 downto 17) := (others => '0')");
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

	ASSERT_EQ(2u, port.pins().size());
	EXPECT_EQ("foo", port.pins().at(0)->name());
}

TEST(VHDLPort, pprint) {
	Port emptyPort;
	Port port(makePort());

	EXPECT_PRETTY_PRINTED(emptyPort, "port( );");
	EXPECT_PRETTY_PRINTED(port, "port(\n    foo : out std_logic;\n    blub : in std_logic\n);");
}


Comment makeComment() {
	return Comment("abc\n\ndef");
}

TEST(VHDLComment, pprint) {
	Comment emptyComment(pprint::Empty::instance());
	Comment emptyLineComment("");
	Comment comment(makeComment());

	EXPECT_HAS_NO_LINES(emptyComment);
	EXPECT_PRETTY_PRINTED(emptyLineComment, "-- ");
	EXPECT_PRETTY_PRINTED(comment, "-- abc\n-- \n-- def");
}

TEST(VHDLComment, prefix) {
	Comment comment1("abc\n\ndef", "");
	Comment comment2("abc\n\ndef", "---");

	EXPECT_PRETTY_PRINTED(comment1, "--abc\n--\n--def");
	EXPECT_PRETTY_PRINTED(comment2, "-----abc\n-----\n-----def");
}


Entity makeEntity() {
	Entity entity("blub");
	entity.port() = makePort();
	return entity;
}

TEST(VHDLEntity, getters) {
	Entity entity(makeEntity());

	EXPECT_EQ("blub", entity.name());
}

TEST(VHDLEntity, pprint) {
	Entity emptyEntity("empty");
	Entity entity(makeEntity());

	EXPECT_PRETTY_PRINTED(emptyEntity,
		"entity empty is\nend empty;");
	EXPECT_PRETTY_PRINTED(entity,
		"entity blub is\n"
		"    port(\n        foo : out std_logic;\n        blub : in std_logic\n    );\n"
		"end blub;");
}


Signal makeSignal1() {
	return Signal("abc", Direction::Default, Type("double"));
}

Signal makeSignal2() {
	return Signal("abc", Direction::Default, Type("double"), Value("(others => '0')"));
}

TEST(VHDLSignal, pprint) {
	EXPECT_PRETTY_PRINTED(makeSignal1(),
		"signal abc : double");

	EXPECT_PRETTY_PRINTED(makeSignal2(),
		"signal abc : double := (others => '0')");

	EXPECT_PRETTY_PRINTED(Signal("x", Direction::In, Type("int")),
		"signal x : in int");

	EXPECT_PRETTY_PRINTED(Signal("x", Type("int")),
		"signal x : int");

	EXPECT_PRETTY_PRINTED(Signal("x", Type("int"), Value("(others => '1')")),
		"signal x : int := (others => '1')");
}


Constant makeConstant1() {
	return Constant("abc", Direction::Default, Type("double"));
}

Constant makeConstant2() {
	return Constant("abc", Direction::Default, Type("double"), Value("(others => '0')"));
}

TEST(VHDLConstant, pprint) {
	EXPECT_PRETTY_PRINTED(makeConstant1(),
		"constant abc : double");

	EXPECT_PRETTY_PRINTED(makeConstant2(),
		"constant abc : double := (others => '0')");

	EXPECT_PRETTY_PRINTED(Constant("x", Direction::In, Type("int")),
		"constant x : in int");

	EXPECT_PRETTY_PRINTED(Constant("x", Type("int")),
		"constant x : int");

	EXPECT_PRETTY_PRINTED(Constant("x", Type("int"), Value("(others => '1')")),
		"constant x : int := (others => '1')");
}


Variable makeVariable1() {
	return Variable("abc", Direction::Default, Type("double"));
}

Variable makeVariable2() {
	return Variable("abc", Direction::Default, Type("double"), Value("(others => '0')"));
}

TEST(VHDLVariable, pprint) {
	EXPECT_PRETTY_PRINTED(makeVariable1(),
		"variable abc : double");

	EXPECT_PRETTY_PRINTED(makeVariable2(),
		"variable abc : double := (others => '0')");

	EXPECT_PRETTY_PRINTED(Variable("x", Direction::In, Type("int")),
		"variable x : in int");

	EXPECT_PRETTY_PRINTED(Variable("x", Type("int")),
		"variable x : int");

	EXPECT_PRETTY_PRINTED(Variable("x", Type("int"), Value("(others => '1')")),
		"variable x : int := (others => '1')");
}


Component makeComponent() {
	Component component("blub");
	component.port() = makePort();
	return component;
}

TEST(VHDLComponent, getters) {
	Component component(makeComponent());

	EXPECT_EQ("blub", component.name());
}

TEST(VHDLComponent, pprint) {
	Component emptyComponent("empty");
	Component component(makeComponent());

	EXPECT_PRETTY_PRINTED(emptyComponent,
		"component empty\nend component;");
	EXPECT_PRETTY_PRINTED(component,
		"component blub\n"
		"    port(\n        foo : out std_logic;\n        blub : in std_logic\n    );\n"
		"end component;");
}


Architecture makeArchitecture1() {
	return Architecture("behavior", "bla");
}

Architecture makeArchitecture2() {
	return Architecture("structural", shared_ptr<Entity>(new Entity(makeEntity())));
}

TEST(VHDLArchitecture, getters) {
	Architecture architecture1(makeArchitecture1());
	Architecture architecture2(makeArchitecture2());

	EXPECT_EQ("behavior",   architecture1.name());
	EXPECT_EQ("structural", architecture2.name());

	EXPECT_EQ("bla",  architecture1.entityName());
	EXPECT_EQ("blub", architecture2.entityName());

	EXPECT_FALSE(architecture1.entity());
	EXPECT_EQ("blub", architecture2.entity()->name());
}

TEST(VHDLArchitecture, declarations) {
	Architecture architecture1(makeArchitecture1());

	boost::shared_ptr<Comment> comment = ptr(Comment("abc"));
	architecture1.addDeclaration(comment);

	ASSERT_EQ(1u, architecture1.declarations().size());
	EXPECT_EQ(comment, architecture1.declarations().at(0));
}

TEST(VHDLArchitecture, pprint) {
	Architecture architecture1(makeArchitecture1());
	Architecture architecture2(makeArchitecture2());

	EXPECT_PRETTY_PRINTED(architecture1,
		"architecture behavior of bla is\nbegin\nend behavior;");
	EXPECT_PRETTY_PRINTED(architecture2,
		"architecture structural of blub is\nbegin\nend structural;");
}

TEST(VHDLArchitecture, testWithSignals) {
	Architecture architecture(makeArchitecture2());

	architecture.addDeclaration(ptr(makeSignal1()));
	architecture.addDeclaration(ptr(makeSignal2()));

	EXPECT_PRETTY_PRINTED(architecture,
		"architecture structural of blub is\n"
		"    signal abc : double;\n"
		"    signal abc : double := (others => '0');\n"
		"begin\nend structural;");
}


TEST(VHDLCompilationUnit, pprint) {
	CompilationUnit file;

	file.libraries()->add("ieee")
		<< "std_logic_1164.all"
		<< "numeric_std.all";

	file.add(new Architecture(makeArchitecture1()));

	EXPECT_PRETTY_PRINTED(file,
		"library ieee;\n"
		"use ieee.numeric_std.all;\n"
		"use ieee.std_logic_1164.all;\n"
		"\n\n"
		"architecture behavior of bla is\nbegin\nend behavior;");
}

TEST(VHDLCompilationUnit, addCommentBeforeLibraries) {
	CompilationUnit file;

	file.add(new Comment("blub"));

	file.libraries()->add("ieee")
		<< "std_logic_1164.all"
		<< "numeric_std.all";

	EXPECT_PRETTY_PRINTED(file,
		"-- blub\n"
		"\n\n"
		"library ieee;\n"
		"use ieee.numeric_std.all;\n"
		"use ieee.std_logic_1164.all;");
}

TEST(VHDLCompilationUnit, onlyCommentsCanBeAddedBeforeLibraries) {
	CompilationUnit file;

	file.add(new Architecture(makeArchitecture1()));

	file.libraries()->add("ieee")
		<< "std_logic_1164.all"
		<< "numeric_std.all";

	EXPECT_PRETTY_PRINTED(file,
		"library ieee;\n"
		"use ieee.numeric_std.all;\n"
		"use ieee.std_logic_1164.all;\n"
		"\n\n"
		"architecture behavior of bla is\nbegin\nend behavior;");
}
