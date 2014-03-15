#include <cctype>
#include "vhdlgen.h"

namespace vhdl {

UsedLibrary::UsedLibrary(std::string name) : name(name) { }

UsedLibrary& UsedLibrary::operator << (std::string element) {
	this->insert(element);
	return *this;
}

const std::string& UsedLibrary::getName() {
	return this->name;
}

UsedLibrary& UsedLibraries::add(std::string name) {
	UsedLibrary lib(name);
	UsedLibrary& lib_in_map = (*this)[name];
	lib_in_map = lib;
	return lib_in_map;
}

const pprint::PrettyPrinted_p UsedLibrary::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;
	builder.append()
		.columns()
			.add("library")
			.add(" ")
			.add(name)
			.add(";")
			.up();

	for (const_iterator iter = this->begin(); iter != this->end(); ++iter)
		builder.columns()
			.add("use")
			.add(" ")
			.add(name + "." + *iter)
			.add(";")
			.up();

	return builder.build();
}

const pprint::PrettyPrinted_p UsedLibraries::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;

	builder.append()
		.seperateBy("");

	for (const_iterator iter = this->begin(); iter != this->end(); ++iter)
		builder.add(iter->second);

	return builder.build();
}


std::string tolower(const std::string& str) {
	std::string copy(str);
	for (std::string::iterator iter = copy.begin(); iter != copy.end(); ++iter)
		*iter = std::tolower(*iter);
	return copy;
}

Direction::Direction(_Direction _direction, std::string text) : _direction(_direction), text(text) { }

const Direction Direction::Default(Direction_Default, "Default");
const Direction Direction::In     (Direction_In,      "In");
const Direction Direction::Out    (Direction_Out,     "Out");
const Direction Direction::InOut  (Direction_InOut,   "InOut");

const pprint::PrettyPrinted_p Direction::prettyPrint() const {
	if (_direction == Direction_Default)
		return pprint::Empty::instance();
	else
		return pprint::Text::create(tolower(text));
}

_Direction Direction::direction() const { return _direction; }

std::string Direction::str() const { return text; }

bool Direction::operator ==(const Direction& dir) const {
	return _direction == dir._direction;
}

bool Direction::operator !=(const Direction& dir) const {
	return _direction != dir._direction;
}


Type::Type(std::string type) : _type(type) { }

Type::~Type() { }

std::string Type::type() const { return _type; }

const pprint::PrettyPrinted_p Type::prettyPrint() const {
	return pprint::Text::create(_type);
}


Value::Value(std::string value) : _value(value) { }

Value::~Value() { }

std::string Value::value() const { return _value; }

const pprint::PrettyPrinted_p Value::prettyPrint() const {
	return pprint::Text::create(_value);
}


ValueDeclaration::ValueDeclaration(std::string name, Direction direction, Type type)
	: _name(name), _direction(direction), _type(type) { }

ValueDeclaration::~ValueDeclaration() { }

void ValueDeclaration::build(pprint::PrettyPrintBuilder& builder) const {
	builder.columns()
		.seperateBy(" ")
		.add(_name)
		.add(":")
		.add(_direction)
		.add(_type)
		.up();
}

const pprint::PrettyPrinted_p ValueDeclaration::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;
	build(builder);
	return builder.build();
}

const std::string& ValueDeclaration::name()      const { return _name;      }
const Direction&   ValueDeclaration::direction() const { return _direction; }
const Type&        ValueDeclaration::type()      const { return _type;      }


ValueDeclarationWithOptionalDefault::ValueDeclarationWithOptionalDefault(
	std::string name, Direction direction, Type type)
	: ValueDeclaration(name, direction, type), _value("") { }

ValueDeclarationWithOptionalDefault::ValueDeclarationWithOptionalDefault(
	std::string name, Direction direction, Type type, Value value)
	: ValueDeclaration(name, direction, type), _value(value) { }

ValueDeclarationWithOptionalDefault::~ValueDeclarationWithOptionalDefault() { }

bool         ValueDeclarationWithOptionalDefault::hasValue()  const { return !_value.value().empty(); }
const Value& ValueDeclarationWithOptionalDefault::value()     const { return _value;     }

void ValueDeclarationWithOptionalDefault::build(pprint::PrettyPrintBuilder& builder) const {
	builder.columns().seperateBy(" ");
	ValueDeclaration::build(builder);

	if (hasValue())
		builder.add(":=").add(_value);

	builder.up();
}


Pin::Pin(std::string name, Direction direction, Type type)
	: ValueDeclaration(name, direction, type) { }


template<typename A, typename B>
std::pair<A, B> pair(const A& a, const B& b) {
	return std::pair<A, B>(a, b);
}

Port& Port::add(boost::shared_ptr<Pin> pin) {
	assert(!contains(pin->name()));

	_byName.insert(pair(pin->name(), pin));
	_order.push_back(pin);

	return *this;
}

Port& Port::add(const Pin& pin) {
	boost::shared_ptr<Pin> pin_ptr(new Pin(pin));

	add(pin_ptr);

	return *this;
}

const std::map<std::string, boost::shared_ptr<Pin> >& Port::pinsByName() const { return _byName; }

const std::vector<boost::shared_ptr<Pin> >& Port::pins() const { return _order; }

bool Port::contains(const std::string& name) const {
	return _byName.find(name) != _byName.end();
}

const Pin& Port::operator[](const std::string& name) const {
	assert(contains(name));

	return *_byName.find(name)->second;
}

const pprint::PrettyPrinted_p Port::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;

	if (_order.empty())
		builder.add("port( );");
	else
		builder.append()
			.add("port(")
			.indent()
				.appendOverlapping()
					.seperateBy(";\n")
					.add(_order.begin(), _order.end())
					.up()
				.up()
			.add(");");

	return builder.build();
}


Comment::Comment(pprint::PrettyPrinted_p content) : _content(content) { }

Comment::Comment(pprint::PrettyPrintable_p content)
	: _content(content->prettyPrint()) { }

Comment::Comment(const std::string& text)
	: _content(pprint::Text::create(text)) { }

const pprint::PrettyPrinted_p Comment::prettyPrint() const {
	return pprint::PrettyPrinted_p(new pprint::Indent("-- ", _content));
}


Entity::Entity(const std::string& name) : _name(name) { }

const std::string& Entity::name() const { return _name; }
Port& Entity::port() { return _port; }

const pprint::PrettyPrinted_p Entity::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;

	builder.append()
		.columns()
			.seperateBy(" ")
			.add("entity")
			.add(name())
			.add("is")
			.up();

	if (!_port.pins().empty())
		builder.indent()
			.add(_port)
			.up();

	builder.columns()
			.add("end ")
			.add(name())
			.add(";")
			.up()
		.up();

	return builder.build();
}

Architecture::Architecture(const std::string& name, const std::string& entityName)
	: _name(name), _entityName(entityName) { }

Architecture::Architecture(const std::string& name, shared_ptr<Entity> entity)
	: _name(name), _entity(entity) { }

const std::string& Architecture::name() const { return _name; }
shared_ptr<Entity> Architecture::entity() { return _entity; }

const std::string& Architecture::entityName() const {
	if (_entity)
		return _entity->name();
	else
		return _entityName;
}

const pprint::PrettyPrinted_p Architecture::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;

	builder.append()
		.columns()
			.seperateBy(" ")
			.add("architecture")
			.add(name())
			.add("of")
			.add(entityName())
			.add("is")
			.up();

	//TODO add declarations

	builder.add("begin");

	//TODO add body

	builder.columns()
			.add("end ")
			.add(name())
			.add(";")
			.up()
		.up();

	return builder.build();
}


CompilationUnit::CompilationUnit()
		: _libraries(new UsedLibraries()) {
	add(_libraries);
}

boost::shared_ptr<UsedLibraries> CompilationUnit::libraries() { return _libraries; }

CompilationUnit& CompilationUnit::add(ToplevelDeclaration* decl) {
	return add(shared_ptr<ToplevelDeclaration>(decl));
}

CompilationUnit& CompilationUnit::add(shared_ptr<ToplevelDeclaration> decl) {
	push_back(decl);

	return *this;
}

const pprint::PrettyPrinted_p CompilationUnit::prettyPrint() const {
	pprint::PrettyPrintBuilder builder;

	builder.append()
		.seperateBy("\n")
		.add(this->begin(), this->end())
		.up();

	return builder.build();
}

}	// end of namespace vhdl
