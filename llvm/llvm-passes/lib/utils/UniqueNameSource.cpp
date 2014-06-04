#include "mehari/utils/UniqueNameSource.h"
#include "mehari/utils/StringUtils.h"

UniqueNameSource::UniqueNameSource(const std::string& prefix)
  : prefix(prefix), counter(0) { }

void UniqueNameSource::reset() {
  counter = 0;
}

std::string UniqueNameSource::next() {
  return (prefix + (counter++));
}


UniqueNameSet::UniqueNameSet() { }

void UniqueNameSet::reset() {
  used_names.clear();
}

bool UniqueNameSet::isUsed(const std::string& name) {
	return (used_names.find(name) != used_names.end());
}

void UniqueNameSet::addUsedName(const std::string& name) {
	used_names.insert(name);
}

std::string UniqueNameSet::makeUnique(const std::string& name) {
	if (!isUsed(name)) {
		addUsedName(name);
		return name;
	}

	int i = 1;
	std::string name2;
	do {
		std::stringstream s;
		s << name << (i++);
		name2 = s.str();
	} while (isUsed(name2));

	addUsedName(name2);

	return name2;
}
