#ifndef MY_OPERATOR_H
#define MY_OPERATOR_H

#include <Operator.hpp>

#include <ios>

class MyOperator : public ::Operator {
public:
  template<typename T>
  inline MyOperator& operator <<(const T& x) {
    vhdl << x;
    return *this;
  }

  inline MyOperator& operator <<(std::ostream&(*f)(std::ostream&)) {
  	vhdl << f;
  	return *this;
  }

  void licence(std::ostream& o, std::string authorsyears);

  void stdLibs(std::ostream& o);
};

#endif /*MY_OPERATOR_H*/
