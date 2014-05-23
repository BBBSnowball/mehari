#ifndef UNIQUE_NAME_SOURCE_H
#define UNIQUE_NAME_SOURCE_H

#include <string>

class UniqueNameSource {
  std::string prefix;
  int counter;
public:
  UniqueNameSource(const std::string& prefix);

  void reset();

  std::string next();
};


#endif /*UNIQUE_NAME_SOURCE_H*/
