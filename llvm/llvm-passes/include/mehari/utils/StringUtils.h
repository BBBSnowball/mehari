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

class Seperator {
	bool first;
	std::string seperator;
public:
	inline Seperator(std::string seperator) : first(true), seperator(seperator) { }

	inline void print(std::ostream& stream) {
		if (first)
			first = false;
		else
			stream << seperator;
	}
};
inline std::ostream& operator <<(std::ostream& stream, Seperator& seperator) {
	seperator.print(stream);
	return stream;
}

#endif /*STRING_UTILS_H*/
