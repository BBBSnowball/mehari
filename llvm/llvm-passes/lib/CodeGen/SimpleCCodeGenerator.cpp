#include "mehari/CodeGen/SimpleCCodeGenerator.h"

#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"

#include <vector>
#include <string>


SimpleCCodeGenerator::SimpleCCodeGenerator() : FunctionPass(ID) {
  opcodeStrings["fadd"] = "+";
  opcodeStrings["fsub"] = "-";
  opcodeStrings["fmul"] = "*";
  opcodeStrings["fdiv"] = "/";
}

SimpleCCodeGenerator::~SimpleCCodeGenerator() {}


bool SimpleCCodeGenerator::runOnFunction(Function &func) {

  errs() << "\n\ngenerate C code for: " << func.getName() << "\n";

  enum ParsingState {ALLOCA, STORE, CALC};
  ParsingState pstate = ALLOCA;

  enum CalcState {LOAD_VAR, GET_INDEX, LOAD_INDEX, APPLY_CALC, STORE_CALC};
  CalcState cstate = LOAD_VAR;

  std::string allocaOpcode = "alloca";
  std::string storeOpcode = "store";
  std::string loadOpcode = "load";
  std::string getelementptrOpcode = "getelementptr"; 
  std::string callOpcode = "call";

  std::vector<Instruction*> worklist;
  for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I)
    worklist.push_back(&*I);

  std::vector< std::vector<std::string> > instructions;
  std::vector<std::string> currentInstruction;

  for (std::vector<Instruction*>::iterator instr_it = worklist.begin(); instr_it != worklist.end(); ++instr_it) {

    Instruction *instr = dyn_cast<Instruction>(*instr_it);

    // errs() << *instr << "\n";

    switch (pstate) {

      case ALLOCA:
        if (allocaOpcode.compare(dyn_cast<Instruction>(*(instr_it+1))->getOpcodeName()) != 0)
          pstate = STORE;
      break;

      case STORE:
        if (storeOpcode.compare(dyn_cast<Instruction>(*(instr_it+1))->getOpcodeName()) != 0)
          pstate = CALC;
        variables[instr->getOperand(1)] = instr->getOperand(0)->getName(); //variables[varaddr] = varname;
      break;

      case CALC:
        
        if ((instr_it+1) == worklist.end())
          break;

        switch (cstate) {

          case LOAD_VAR:
            currentInstruction.push_back(variables[instr->getOperand(0)]);
            if (getelementptrOpcode.compare(dyn_cast<Instruction>(*(instr_it+1))->getOpcodeName()) == 0)
              cstate = GET_INDEX;
          break;

          case GET_INDEX:
            if (llvm::ConstantInt* CI = dyn_cast<llvm::ConstantInt>(instr->getOperand(1))) {
              currentInstruction.push_back(CI->getValue().toString(10, true));
            }
            if (loadOpcode.compare(dyn_cast<Instruction>(*(instr_it+1))->getOpcodeName()) == 0)
              cstate = LOAD_INDEX;
            else if (storeOpcode.compare(dyn_cast<Instruction>(*(instr_it+1))->getOpcodeName()) == 0)
              cstate = STORE_CALC;
          break;

          case LOAD_INDEX:
            if (loadOpcode.compare(dyn_cast<Instruction>(*(instr_it+1))->getOpcodeName()) == 0)
              cstate = LOAD_VAR;
            else
              cstate = APPLY_CALC;
          break;

          case APPLY_CALC:
            if (callOpcode.compare(instr->getOpcodeName()) == 0) 
              currentInstruction.push_back(instr->getOperand(1)->getName());
            else
              currentInstruction.push_back(instr->getOpcodeName());
            if (loadOpcode.compare(dyn_cast<Instruction>(*(instr_it+1))->getOpcodeName()) == 0)
              cstate = LOAD_VAR;
          break; 

          case STORE_CALC:
            instructions.push_back(currentInstruction);
            currentInstruction.clear();
            cstate = LOAD_VAR;
          break;

        }
      break;
    }     
  }

  errs() << "\n";
  for (std::vector< std::vector<std::string> >::iterator instr_it = instructions.begin(); instr_it != instructions.end(); ++instr_it)
    errs() << createInstruction(*instr_it) << "\n";
  errs() << "\n";

  return false;
}


std::string SimpleCCodeGenerator::createInstruction(std::vector<std::string> instructionVector) {
  std::string instructionLeft;
  std::string instructionRight;
  std::string currentVariable;
  std::vector<std::string> currentVariables;
  std::vector<std::string> currentOperations;
  bool isVariable;
  bool calculationDone = false;
  enum ParsingState {VARIABLE, OPERATION};
  ParsingState state;

  // for (std::vector<std::string>::iterator it = instructionVector.begin(); it != instructionVector.end()-2; ++it)
  //   errs() << *it << " ";
  // errs() << "\n";

  instructionLeft = instructionVector[instructionVector.size()-2] + "[" + instructionVector[instructionVector.size()-1] + "] = "; 

  for (std::vector<std::string>::iterator it = instructionVector.begin(); it != instructionVector.end()-2; ++it) {

    isVariable = false;
    std::map <Value*, std::string>::iterator i = variables.begin();
    while(i != variables.end()) {
        if(isVariable = (i->second.compare(*it) == 0))
            break;
        ++i;
    }

    if (isVariable) {
      if (calculationDone) {
        std::string tmpString;
        for (std::vector<std::string>::reverse_iterator varIt = currentVariables.rbegin(); varIt != currentVariables.rend(); ++varIt) {
          if (currentOperations.size() != 0) {
            tmpString = currentOperations.front() + "(" + *varIt + ")" + tmpString;
            currentOperations.erase(currentOperations.begin());
          }
          else
            tmpString = *varIt + tmpString;
        }
        while (!currentOperations.empty()) {
          tmpString = currentOperations.front() + "(" + tmpString + ")";
          currentOperations.erase(currentOperations.begin());
        }
        instructionRight += tmpString;
        currentVariables.clear();
        currentOperations.clear();
        calculationDone = false;
      }
      currentVariable.clear();
      currentVariable += *it;
      state = VARIABLE;
    }
    else {
      switch (state) {
        case VARIABLE:
          currentVariable += "[" + *it + "]";
          currentVariables.push_back(currentVariable);
          state = OPERATION;
        break;

        case OPERATION:
          if ((*it).compare("fdiv") == 0) {
            if (instructionRight.empty())
              instructionRight = "1.0";
            instructionRight = "(" + instructionRight + ")";
          }
          currentOperations.push_back(parseOperation(*it));
          calculationDone = true;
        break;
      }
    }
  }
  std::string tmpString;
  for (std::vector<std::string>::reverse_iterator varIt = currentVariables.rbegin(); varIt != currentVariables.rend(); ++varIt) {
    if (currentOperations.size() != 0) {
      tmpString = currentOperations.front() + "(" + *varIt + ")" + tmpString;
      currentOperations.erase(currentOperations.begin());
    }
    else
      tmpString = *varIt + tmpString;
  }
  instructionRight += tmpString;
  return instructionLeft + instructionRight + ";";
}


std::string SimpleCCodeGenerator::parseOperation(std::string opcode) {
  if (opcodeStrings.find(opcode) != opcodeStrings.end())
    return opcodeStrings[opcode];
  else
    return opcode;
}


void SimpleCCodeGenerator::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}


// register pass so we can call it using opt
char SimpleCCodeGenerator::ID = 0;
static RegisterPass<SimpleCCodeGenerator>
Y("ccode", "Generate simple C code from LLVM IR. This will only work for especially preparted sources.");
