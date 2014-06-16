#ifndef MY_OPERATOR_H
#define MY_OPERATOR_H

class MyOperator : public ::Operator {
public:
  template<typename T>
  inline void addCode(const T& x) {
    vhdl << x;
  }

  void licence(std::ostream& o, std::string authorsyears);

  void stdLibs(std::ostream& o);
};

template<typename T>
inline MyOperator& operator <<(MyOperator& op, const T& x) {
  op.addCode(x);
  return op;
}

#endif /*MY_OPERATOR_H*/
