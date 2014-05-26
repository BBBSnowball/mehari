#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <sstream>

template<typename T>
inline static std::string toString(const T& value) {
  std::stringstream ss;
  ss << value;
  return ss.str();
}

inline static std::string operator+(const std::string& a, unsigned int b) {
  std::ostringstream stream;
  stream << a;
  stream << b;
  return stream.str();
}

#endif /*STRING_UTILS_H*/
