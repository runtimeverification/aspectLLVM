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

  struct CallInstInstrPoint {
    CallInstInstrPoint(CallInst* c, std::string h, Value* a = NULL) {
      cinst = c; hookName = h; after = a;
    }
    CallInst* cinst;
    std::string hookName;
    Value* after;
  };

  std::vector<CallInstInstrPoint*> callInstPoints; 

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
      callInstPoints.clear();
      std::map<std::string, std::string>::iterator i,ie; 
      if (!beforeExec.empty()) {
        for (i = beforeExec.begin(), ie = beforeExec.end(); i != ie; i++) {
          std::cerr << "Inserting hook '" << i->second 
            << "' before executing the body of '" << i->first << "'"
            << std::endl;
          hookBeforeExecute(M, i->first, i->second);
        }
      }

      if (!beforeCall.empty() || !afterCall.empty()) {
        for (Module::iterator fi = M.begin(), fe = M.end(); 
            fi != fe; fi++) { 
          for (Function::iterator bbi = fi->begin(), bbe = fi->end();
              bbi != bbe; bbi++) {
            for (BasicBlock::iterator ii = bbi->begin(), ie = bbi->end();
                ii != ie; ii++) {
              if (CallInst *cinst = dyn_cast<CallInst>(ii)) {
                Function* fn = cinst->getCalledFunction();
                std::string fnName(fn->getName().data());
                if ((i= beforeCall.find(fnName)) != beforeCall.end()) {
                  callInstPoints.push_back(
                      new CallInstInstrPoint(cinst, i->second));
                  //std::cerr << "Calling " << i->second 
                    //<< " before calling " << fnName << std::endl;
                }
                if ((i= afterCall.find(fnName)) != afterCall.end()) {
                  BasicBlock::iterator nexti = ii; nexti++;
                  Value* after;
                  if (nexti == ie) {
                    after = bbi;
                  } else {
                    after = nexti;
                  }
                  callInstPoints.push_back(
                      new CallInstInstrPoint(cinst, i->second, after));
                  //std::cerr << "Calling " << i->second 
                    //<< " after calling " << fnName << std::endl;
                }
              }
            }
          }
        }
      }

      std::vector<CallInstInstrPoint*>::iterator ipi,ipe;
      for (ipi = callInstPoints.begin(), ipe = callInstPoints.end();
          ipi != ipe; ipi++) {
        CallInstInstrPoint* ip = *ipi;
        instrumentCallPoint(M, ip);
     }
      return true;
    }


    void instrumentCallPoint(Module& M,
        CallInstInstrPoint* ip) {
      CallInst* cinst = ip->cinst;
      std::cerr << "Calling " << ip->hookName;
      if (ip->after) std::cerr << " after ";
      else std::cerr << " before ";
      Function* fn = cinst->getCalledFunction();
      std::string fnName(fn->getName().data());
      std::cerr << " calling " << fnName << std::endl;
      FunctionType* ft = fn->getFunctionType();
      std::vector<Type *> types(ft->param_begin(), ft->param_end());
      bool addReturn = ip->after && !ft->getReturnType()->isVoidTy();
      if (addReturn) {
        types.insert(types.begin(), ft->getReturnType());
      }
      FunctionType* hookFT = FunctionType::get(
          Type::getVoidTy(M.getContext()), types, ft->isVarArg());
      Constant *hookFunc = M.getOrInsertFunction(ip->hookName.c_str(), 
          hookFT);
      Function* hook = cast<Function>(hookFunc);
      std::vector<Value *> args;
      if (addReturn) {
        args.push_back(cinst->getCalledValue());
      }
      for (unsigned i = 0, e = cinst->getNumArgOperands(); 
          i != e; i++) {
        args.push_back(cinst->getArgOperand(i));
      }
      if (ip->after) {
        if (CallInst *ci = dyn_cast<CallInst>(ip->after)) {
          CallInst::Create(hook, args, "", ci); 
        } else {
          BasicBlock *bb = cast<BasicBlock>(ip->after);
          CallInst::Create(hook, args, "", bb); 
        }
      } else {
        CallInst::Create(hook, args, "", cinst); 
      }
    }

    void hookBeforeExecute(Module& M, 
        std::string function, 
        std::string hookFn) {
      Function *F = M.getFunction(function.c_str());
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
      CallInst::Create(hook, args, "", (Instruction *)BI); 

    }
  };
}

char AOP::ID = 0;
static RegisterPass<AOP> X("aop", "AOP World Pass");

