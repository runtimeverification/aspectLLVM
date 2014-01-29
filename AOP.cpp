#define DEBUG_TYPE "aop"
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
using namespace llvm;

namespace {
  struct AOP : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    AOP() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      std::ifstream str("aspect.map");
      std::map<std::string, std::string> mymap; 
      std::string key,value;
      while ((str >> key)) {
        str >> value;
        mymap.insert(std::make_pair(key, value));
      }
      std::map<std::string, std::string>::iterator i,ie; 
      for (i = mymap.begin(), ie = mymap.end(); i != ie; i++) {
        std::cerr << "Inserting hook '" << i->second 
          << "' before executing the body of '" << i->first << "'"
          << std::endl;
        hookBeforeExecute(M, i->first, i->second);
      }

      
      return true;
    }

    void hookBeforeExecute(Module& M, 
        std::string function, 
        std::string hookFn) {
      Function *F = M.getFunction(function.c_str());
      Constant *hookFunc = M.getOrInsertFunction(hookFn.c_str(), 
          Type::getVoidTy(M.getContext()), (Type*) 0);

      Function* hook = cast<Function>(hookFunc);
      BasicBlock &BB = F->getEntryBlock();

      BasicBlock::iterator BI = BB.begin();

      CallInst::Create(hook, "", (Instruction *)BI); 

    }
  };
}

char AOP::ID = 0;
static RegisterPass<AOP> X("aop", "AOP World Pass");

