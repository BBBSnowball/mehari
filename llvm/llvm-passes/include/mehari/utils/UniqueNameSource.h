#ifndef UNIQUE_NAME_SOURCE_H
#define UNIQUE_NAME_SOURCE_H

#include <string>
#include <set>
#include <sstream>

class UniqueNameSource {
  std::string prefix;
  int counter;
public:
  UniqueNameSource(const std::string& prefix);

  void reset();

  std::string next();
};

class UniqueNameSet {
  std::set<std::string> used_names;
public:
  UniqueNameSet();

  void reset();

  bool isUsed(const std::string& name);

  void addUsedName(const std::string& name);
  std::string makeUnique(const std::string& name);
};

#endif /*UNIQUE_NAME_SOURCE_H*/
