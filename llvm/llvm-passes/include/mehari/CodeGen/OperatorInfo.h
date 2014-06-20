#ifndef OPERATOR_INFO_H
#define OPERATOR_INFO_H

#include "Operator.hpp"

#include <string>

struct OperatorInfo {
  ::Operator* op;
  std::string input1, input2, output;
  std::string data_suffix, valid_suffix, ready_suffix;
  std::string code;
};

#endif /*OPERATOR_INFO_H*/
