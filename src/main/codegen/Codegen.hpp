#ifndef CODEGEN_CODEGEN
#define CODEGEN_CODEGEN

#include <map>
#include <llvm/IR/IRBuilder.h>
#include "common/NodeManager.hpp"

class Codegen {
public:
  llvm::LLVMContext ctx;
  llvm::IRBuilder<>* b;
  llvm::Module* mod;
  const NodeManager* nodeManager;

  /** Maps parameter names to their LLVM values for a single function (the one
   * currently being generated). */
  std::map<std::string, llvm::Value*> functionParams;

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
      default: return b->getVoidTy();
    }
  }

  llvm::FunctionType* signatureToFunctionType(unsigned int _n) {
    Node n = nodeManager->get(_n);
    std::vector<llvm::Type*> paramTys;
    Node paramList = nodeManager->get(n.n1);
    while (paramList.ty == NodeTy::PARAMLIST_CONS) {
      paramTys.push_back(genType(paramList.n2));
      paramList = nodeManager->get(paramList.n3);
    }
    llvm::Type* retTy = genType(n.n2);
    return llvm::FunctionType::get(retTy, paramTys, false);
  }

  /** Sets the `functionParams` class variable. Also sets argument
   * names in the LLVM IR. */
  void setFunctionParams(llvm::Function* f, unsigned int _signature) {
    functionParams.clear();
    Node paramList = nodeManager->get(nodeManager->get(_signature).n1);
    for (llvm::Argument &arg : f->args()) {
      Node paramName = nodeManager->get(paramList.n1);
      std::string paramNameStr(paramName.loc.ptr, paramName.loc.sz);
      functionParams[paramNameStr] = &arg;
      arg.setName(paramNameStr);        // Not strictly necessary, but helpful to have
      paramList = nodeManager->get(paramList.n3);
    }
  }

  llvm::Function* genProc(unsigned int _n) {
    Node n = nodeManager->get(_n);
    Node nameNode = nodeManager->get(n.n1);
    
    std::string funcName(nameNode.loc.ptr, nameNode.loc.sz);

    llvm::Function* f = llvm::Function::Create(
      signatureToFunctionType(n.n2),
      llvm::Function::ExternalLinkage,
      funcName,
      mod
    );
    
    setFunctionParams(f, n.n2);

    b->SetInsertPoint(llvm::BasicBlock::Create(ctx, "entry", f));
    llvm::Value* retVal = genExp(n.n3);
    b->CreateRet(retVal);
    return f;
  }

  llvm::Value* genExp(unsigned int _n) {
    Node n = nodeManager->get(_n);
    if (n.ty == NodeTy::IDENT) {
      std::string identStr(n.loc.ptr, n.loc.sz);
      llvm::Value* v = functionParams[identStr];
      if (!v) {
        printf("Could not get argument!!! %s\n", identStr.c_str());
        exit(1);
      }
      return v;
    }

    else if (n.ty == NodeTy::LIT_INT) {
      std::string litText(n.loc.ptr, n.loc.sz);
      int number = std::stoi(litText);
      return b->getInt32(number);
    }

    else if (n.ty == NodeTy::ADD) {
      llvm::Value* v1 = genExp(n.n1);
      llvm::Value* v2 = genExp(n.n2);
      return b->CreateAdd(v1, v2);
    }

    else if (n.ty == NodeTy::MUL) {
      llvm::Value* v1 = genExp(n.n1);
      llvm::Value* v2 = genExp(n.n2);
      return b->CreateMul(v1, v2);
    }

    else {
      printf("An error occured in genExp!\n");
      exit(1);
    }
  }
};

#endif