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
    AOP() : ModulePass(ID) {
      readConfig();
    }
    std::map<std::string, std::string> beforeExec; 
    std::map<std::string, std::string> beforeCall; 
    std::map<std::string, std::string> afterCall; 

    void readConfig() {
      std::ifstream str("aspect.map");
      std::string key,value,when,what,action;
      while ((str >> when)) {
        assert (when == "before" || when == "after");
        str >> what;
        assert (what == "executing" || what == "calling");
        str >> key;
        str >> action;
        assert (action == "call");
        str >> value;
        if (when == "before") {
          if (what == "executing") {
            beforeExec.insert(std::make_pair(key, value));
          } else if (what == "calling") {
            beforeCall.insert(std::make_pair(key, value));
          } else {
            fail(when, what);
          }
        } else if (when == "after") {
          if (what == "calling") {
            afterCall.insert(std::make_pair(key, value));
          } else {
            fail(when, what);
          }
        } else {
          fail(when, what);
        }
      }
    }

    void fail(std::string when, std::string what) {
            std::cerr << "Instrumentation point " << when << " " << what 
              << " not supported!" << std::endl;
    }

    virtual bool runOnModule(Module &M) {
      std::map<std::string, std::string>::iterator i,ie; 
      if (!beforeExec.empty()) {
        for (i = beforeExec.begin(), ie = beforeExec.end(); i != ie; i++) {
          std::cerr << "Inserting hook '" << i->second 
            << "' before executing the body of '" << i->first << "'"
            << std::endl;
          hookBeforeExecute(M, i->first, i->second);
        }
      }

      
      return true;
    }

    void hookBeforeExecute(Module& M, 
        std::string function, 
        std::string hookFn) {
      Function *F = M.getFunction(function.c_str());
      /*
      Constant *hookFunc = M.getOrInsertFunction(hookFn.c_str(), 
          Type::getVoidTy(M.getContext()), (Type*) 0);
          */
      Constant *hookFunc = M.getOrInsertFunction(hookFn.c_str(), 
          F->getFunctionType());

      Function* hook = cast<Function>(hookFunc);
      BasicBlock &BB = F->getEntryBlock();

      BasicBlock::iterator BI = BB.begin();

      std::vector<Value *> args;
      for (Function::arg_iterator i = F->arg_begin(), e = F-> arg_end();
          i != e; i++) {
        args.push_back(i);
      }
      //CallInst::Create(hook, "", (Instruction *)BI); 
      CallInst::Create(hook, args, "", (Instruction *)BI); 

    }
  };
}

char AOP::ID = 0;
static RegisterPass<AOP> X("aop", "AOP World Pass");

