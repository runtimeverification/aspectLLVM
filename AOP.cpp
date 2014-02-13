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
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;

namespace {

  enum ReturnType { NONE, CAPTURE, SAME };

  struct CallInstInstrPoint {
    CallInstInstrPoint(CallInst* c, std::string h, Value* a = NULL) {
      cinst = c; hookName = h; after = a;
    }
    CallInst* cinst;
    std::string hookName;
    Value* after;
  };

  struct AOP : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    AOP() : ModulePass(ID) {
      readConfig();
    }
    std::map<std::string, std::string> beforeExec; 
    std::map<std::string, std::string> beforeCall; 
    std::map<std::string, std::string> afterCall; 
    std::map<std::string, std::string> insteadOfCall; 

    std::vector<CallInstInstrPoint*> callInstPoints; 
    std::vector<CallInstInstrPoint*> insteadOfInstPoints; 

    void readConfig() {
      std::ifstream str("aspect.map");
      std::string key,value,when,what,action;
      while ((str >> when)) {
        assert (when == "before" || when == "after" || "instead-of");
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
        } else if (when == "instead-of") {
          if (what == "calling") {
            insteadOfCall.insert(std::make_pair(key, value));
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
      insteadOfInstPoints.clear();
      std::map<std::string, std::string>::iterator i,ie; 
      if (!beforeExec.empty()) {
        for (i = beforeExec.begin(), ie = beforeExec.end(); i != ie; i++) {
          std::cerr << "Inserting call to '" << i->second 
            << "' before executing the body of '" << i->first << "'"
            << std::endl;
          hookBeforeExecute(M, i->first, i->second);
        }
      }

      if (!beforeCall.empty() || !afterCall.empty() 
          || !insteadOfCall.empty()) {
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
                if ((i = insteadOfCall.find(fnName)) 
                    != insteadOfCall.end()) {
                  insteadOfInstPoints.push_back(
                      new CallInstInstrPoint(cinst, i->second));
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
      for (ipi = insteadOfInstPoints.begin(), 
          ipe = insteadOfInstPoints.end(); ipi != ipe; ipi++) {
        CallInstInstrPoint* ip = *ipi;
        replaceCallPoint(M, ip);
      }

      return true;
    }

    void setArgs(CallInst* cinst, 
        bool addReturn,
        std::vector<Value *>& args
        ) {
      if (addReturn) {
        args.push_back(cinst);
      }
      for (unsigned i = 0, e = cinst->getNumArgOperands(); 
          i != e; i++) {
        args.push_back(cinst->getArgOperand(i));
      }
    }

    Function* getHookFunction(Module& M, 
        FunctionType* ft,
        const char* name,
        ReturnType retType) {
      std::vector<Type *> types(ft->param_begin(), ft->param_end());
      Type* returnType;
      switch (retType) {
        case CAPTURE: {
                        Type* fnRetType = ft->getReturnType();
                        if (!fnRetType->isVoidTy())
                          types.insert(types.begin(), fnRetType);
                      }
        case NONE:
          returnType = Type::getVoidTy(M.getContext());
          break;
        case SAME:
          returnType = ft->getReturnType();
      }
      FunctionType* hookFT = FunctionType::get(
          returnType, types, ft->isVarArg());
      Constant *hookFunc = M.getOrInsertFunction(name, 
          hookFT);
      return cast<Function>(hookFunc);
    }

    void instrumentCallPoint(Module& M, CallInstInstrPoint* ip) {
      CallInst* cinst = ip->cinst;
      std::cerr << "Inserting call to '" << ip->hookName << "' ";
      if (ip->after) std::cerr << "after";
      else std::cerr << "before";
      Function* fn = cinst->getCalledFunction();
      FunctionType* ft = fn->getFunctionType();
      std::string fnName(fn->getName().data());
      std::cerr << " calling '" << fnName << "'." << std::endl;
      bool addReturn = ip->after && !ft->getReturnType()->isVoidTy();
      Function* hook = getHookFunction(M, ft, ip->hookName.c_str(), 
          addReturn ? CAPTURE : NONE);
      std::vector<Value *> args;
      setArgs(cinst, addReturn, args);
      if (ip->after) {
        if (Instruction *before = dyn_cast<Instruction>(ip->after)) {
          CallInst::Create(hook, args, "", before); 
        } else {
          BasicBlock *at_end = cast<BasicBlock>(ip->after);
          CallInst::Create(hook, args, "", at_end); 
        }
      } else {
        CallInst::Create(hook, args, "", cinst); 
      }
    }

    void replaceCallPoint(Module& M, CallInstInstrPoint* ip) {
      CallInst* cinst = ip->cinst;
      Function* fn = cinst->getCalledFunction();
      FunctionType* ft = fn->getFunctionType();
      std::string fnName(fn->getName().data());
      std::cerr << "Replacing call to '" << fnName 
        << "' by '" << ip->hookName << "'." << std::endl;
      Function* hook = getHookFunction(M, ft, ip->hookName.c_str(), SAME);
      std::vector<Value *> args;
      setArgs(cinst, false, args);
      BasicBlock::iterator ii(cinst);
      ReplaceInstWithInst(cinst->getParent()->getInstList(), ii,
          CallInst::Create(hook, args, cinst->getName()));
    }

    void hookBeforeExecute(Module& M, 
        std::string function, 
        std::string hookFn) {
      Function *F = M.getFunction(function.c_str());
      if (F == NULL) return;
      if (F->empty()) return; // function body not present.
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
static RegisterPass<AOP> X("aop", "Aspect Instrumentation Pass");

