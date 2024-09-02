#ifndef CODEGEN_CODEGEN
#define CODEGEN_CODEGEN

#include <llvm/IR/IRBuilder.h>
#include "common/ScopeStack.hpp"
#include "common/NodeManager.hpp"

class Codegen {
public:
  llvm::LLVMContext ctx;
  llvm::IRBuilder<>* b;
  llvm::Module* mod;
  const NodeManager* nodeManager;

  /** Maps variable names to their LLVM values */
  ScopeStack<llvm::Value*> varValues;

  Codegen(const NodeManager* nodeManager) {
    b = new llvm::IRBuilder<>(ctx);
    mod = new llvm::Module("MyModule", ctx);
    this->nodeManager = nodeManager;
  }

  ~Codegen() {
    delete mod;
    delete b;
  }

  llvm::Type* genType(unsigned int _n) {
    Node n = nodeManager->get(_n);
    switch (n.ty) {
      case NodeTy::BOOL: return b->getInt1Ty();
      case NodeTy::i8: return b->getInt8Ty();
      case NodeTy::i32: return b->getInt32Ty();
      case NodeTy::f32: return b->getFloatTy();
      case NodeTy::f64: return b->getDoubleTy();
      case NodeTy::STRING: return b->getInt8PtrTy();
      case NodeTy::UNIT: return b->getVoidTy();
      default: return b->getVoidTy();
    }
  }

  llvm::FunctionType* makeFunctionType(unsigned int _paramList, unsigned int _retTy) {
    Node paramList = nodeManager->get(_paramList);
    std::vector<llvm::Type*> paramTys;
    while (paramList.ty == NodeTy::PARAMLIST_CONS) {
      paramTys.push_back(genType(paramList.n2));
      paramList = nodeManager->get(paramList.n3);
    }
    llvm::Type* retTy = genType(_retTy);
    return llvm::FunctionType::get(retTy, paramTys, false);
  }

  /** Sets the `functionParams` class variable. Also sets argument
   * names in the LLVM IR. */
  void setFunctionParams(llvm::Function* f, unsigned int _paramList) {
    Node paramList = nodeManager->get(_paramList);
    for (llvm::Argument &arg : f->args()) {
      Node paramName = nodeManager->get(paramList.n1);
      std::string paramNameStr(paramName.extra.ptr, paramName.loc.sz);
      varValues.add(paramNameStr, &arg);
      arg.setName(paramNameStr);        // Not strictly necessary, but helpful to have
      paramList = nodeManager->get(paramList.n3);
    }
  }

  llvm::Function* genProc(unsigned int _n) {
    Node n = nodeManager->get(_n);
    Node nameNode = nodeManager->get(n.n1);
    
    std::string funcName(nameNode.extra.ptr, nameNode.loc.sz);

    llvm::Function* f = llvm::Function::Create(
      makeFunctionType(n.n2, n.n3),
      llvm::Function::ExternalLinkage,
      funcName,
      mod
    );
    
    varValues.push();
    setFunctionParams(f, n.n2);
    b->SetInsertPoint(llvm::BasicBlock::Create(ctx, "entry", f));
    llvm::Value* retVal = genExp(n.extra.nodes.n4);
    b->CreateRet(retVal);
    varValues.pop();
    return f;
  }

  llvm::Value* genExp(unsigned int _n) {
    Node n = nodeManager->get(_n);

    if (n.ty == NodeTy::BLOCK) {
      Node stmtList = nodeManager->get(n.n2);
      llvm::Value* lastStmtVal = nullptr;
      while (stmtList.ty == NodeTy::STMTLIST_CONS) {
        lastStmtVal = genStmt(stmtList.n1);
        stmtList = nodeManager->get(stmtList.n2);
      }
      return lastStmtVal;
    }

    else if (n.ty == NodeTy::EIDENT) {
      std::string identStr(n.extra.ptr, n.loc.sz);
      llvm::Value* v = varValues.getOrElse(identStr, nullptr);
      if (!v) {
        printf("Could not get argument!!! %s\n", identStr.c_str());
        exit(1);
      }
      return v;
    }

    else if (n.ty == NodeTy::LIT_INT) {
      return b->getInt32((int)n.extra.intVal);
    }

    else if (n.ty == NodeTy::LIT_STRING) {
      std::string s(n.extra.ptr+1, n.loc.sz-2);
      llvm::Constant* llvmStr = b->CreateGlobalStringPtr(s);
      return llvmStr;
    }

    else if (n.ty == NodeTy::ADD) {
      llvm::Value* v1 = genExp(n.n2);
      llvm::Value* v2 = genExp(n.n3);
      return b->CreateAdd(v1, v2);
    }

    else if (n.ty == NodeTy::MUL) {
      llvm::Value* v1 = genExp(n.n2);
      llvm::Value* v2 = genExp(n.n3);
      return b->CreateMul(v1, v2);
    }

    else {
      printf("An error occured in genExp!\n");
      exit(1);
    }
  }

  llvm::Value* genStmt(unsigned int _n) {
    Node n = nodeManager->get(_n);

    if (n.ty == NodeTy::LET) {
      llvm::Value* v = genExp(n.n2);
      Node n1 = nodeManager->get(n.n1);
      std::string identStr(n1.extra.ptr, n1.loc.sz);
      v->setName(identStr);   // Not strictly necessary, but helpful to have
      varValues.add(identStr, v);
      return nullptr;
    }

    else if (isExpNodeTy(n.ty)) {
      return genExp(_n);
    }

    else {
      printf("Unsupported statement!\n");
      exit(1);
    }

  }

};

#endif