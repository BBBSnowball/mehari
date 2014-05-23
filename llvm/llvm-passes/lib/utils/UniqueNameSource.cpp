#include "mehari/utils/UniqueNameSource.h"
#include "mehari/utils/string_utils.h"

UniqueNameSource::UniqueNameSource(const std::string& prefix)
  : prefix(prefix), counter(0) { }

void UniqueNameSource::reset() {
  counter = 0;
}

std::string UniqueNameSource::next() {
  return (prefix + (counter++));
}
