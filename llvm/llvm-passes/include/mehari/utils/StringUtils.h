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

inline static std::string toString(std::ostream&(*f)(std::ostream&)) {
  std::stringstream ss;
  ss << f;
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

inline static bool endsWith(const std::string& str, const std::string& suffix) {
	return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

inline static std::string replace(const std::string& pattern, const std::string& replacement, const std::string& text,
		int max_replacements = -1) {
	std::string changed_text = text;
	size_t from_pos = 0;
	size_t pos;
	while ((pos = changed_text.find(pattern, from_pos)) != std::string::npos && max_replacements != 0) {
		changed_text.replace(pos, pattern.size(), replacement);
		if (max_replacements > 0)
			max_replacements--;
		from_pos = pos + pattern.size();
	}
	return changed_text;
}

inline static std::string prefixAllLines(const std::string& prefix, const std::string& text) {
	return prefix + replace(toString(std::endl), toString(std::endl)+prefix, text);
}

#endif /*STRING_UTILS_H*/
