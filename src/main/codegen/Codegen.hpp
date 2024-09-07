#ifndef CODEGEN_CODEGEN
#define CODEGEN_CODEGEN

#include <llvm/IR/IRBuilder.h>
#include "common/ScopeStack.hpp"
#include "common/NodeManager.hpp"
#include "common/Ontology.hpp"

class Codegen {
public:
  llvm::LLVMContext ctx;
  llvm::IRBuilder<>* b;
  llvm::Module* mod;
  const NodeManager* nodeManager;
  const Ontology* ont;

  /** Maps variable names to their LLVM values */
  ScopeStack<llvm::Value*> varValues;

  Codegen(const NodeManager* nodeManager, const Ontology* ont) {
    b = new llvm::IRBuilder<>(ctx);
    mod = new llvm::Module("MyModule", ctx);
    mod->setTargetTriple("x86_64-pc-linux-gnu");
    this->nodeManager = nodeManager;
    this->ont = ont;
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

  /** Adds the parameters in `_paramList` to the topmost scope in the scope stack.
   * Also sets argument names in the LLVM IR. */
  void addParamsToScope(llvm::Function* f, unsigned int _paramList) {
    Node paramList = nodeManager->get(_paramList);
    for (llvm::Argument &arg : f->args()) {
      Node paramName = nodeManager->get(paramList.n1);
      std::string paramNameStr(paramName.extra.ptr, paramName.loc.sz);
      varValues.add(paramNameStr, &arg);
      arg.setName(paramNameStr);        // Not strictly necessary, but helpful to have
      paramList = nodeManager->get(paramList.n3);
    }
  }

  llvm::Value* genExp(unsigned int _n) {
    Node n = nodeManager->get(_n);

    // TODO: continue working on this
    if (n.ty == NodeTy::ARRAY_CONSTR_INIT) {
      llvm::Type* innerType = genType(nodeManager->get(n.n1).n1);
      llvm::Value* arraySize = genExp(n.n2);
      return b->CreateAlloca(innerType, arraySize);
    }

    else if (n.ty == NodeTy::BLOCK) {
      Node stmtList = nodeManager->get(n.n2);
      llvm::Value* lastStmtVal = nullptr;
      while (stmtList.ty == NodeTy::STMTLIST_CONS) {
        lastStmtVal = genStmt(stmtList.n1);
        stmtList = nodeManager->get(stmtList.n2);
      }
      return lastStmtVal;
    }

    else if (n.ty == NodeTy::CALL) {
      Node efqident = nodeManager->get(n.n2);
      const char* funcName = efqident.extra.ptr;
      llvm::Function* callee = mod->getFunction(funcName);

      std::vector<llvm::Value*> args;
      Node expList = nodeManager->get(n.n3);
      while (expList.ty == NodeTy::EXPLIST_CONS) {
        args.push_back(genExp(expList.n1));
        expList = nodeManager->get(expList.n2);
      }
      return b->CreateCall(callee, args);
    }

    else if (n.ty == NodeTy::EQ) {
      llvm::Value* v1 = genExp(n.n2);
      llvm::Value* v2 = genExp(n.n3);

      llvm::Type* operandType = v1->getType();
      if (operandType->isIntegerTy())
        return b->CreateICmpEQ(v1, v2);
      else if (operandType->isFloatingPointTy())
        return b->CreateFCmpOEQ(v1, v2);
      else {
        printf("Fatal error: Didn't expect this type for an EQ expression\n");
        exit(1);
      }
    }

    else if (n.ty == NodeTy::EQIDENT) {
      Node identNode = nodeManager->get(n.n2);
      if (identNode.ty == NodeTy::IDENT) {
        std::string identStr(identNode.extra.ptr, identNode.loc.sz);
        return varValues.getOrElse(identStr, nullptr);
      } else {
        printf("Fatal error: Didn't expect qualified identifier in Codegen::genExp\n");
        exit(1);
      }
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

  /** Codegens a `func`, `proc`, `extern func` or `extern proc`. */
  llvm::Function* genFunc(unsigned int _n) {
    Node n = nodeManager->get(_n);
    
    std::string funcName(nodeManager->get(n.n1).extra.ptr);

    llvm::Function* f = llvm::Function::Create(
      makeFunctionType(n.n2, n.n3),
      llvm::Function::ExternalLinkage,
      funcName,
      mod
    );

    if (n.ty == NodeTy::FUNC || n.ty == NodeTy::PROC) {
      varValues.push();
      addParamsToScope(f, n.n2);
      b->SetInsertPoint(llvm::BasicBlock::Create(ctx, "entry", f));
      llvm::Value* retVal = genExp(n.extra.nodes.n4);
      b->CreateRet(retVal);
      varValues.pop();
    }

    return f;
  }

  void genModuleOrNamespace(unsigned int _n) {
    Node n = nodeManager->get(_n);
    genDeclList(n.n2);
  }

  void genDecl(unsigned int _n) {
    Node n = nodeManager->get(_n);

    if (n.ty == NodeTy::MODULE || n.ty == NodeTy::NAMESPACE) {
      genModuleOrNamespace(_n);
    } else if (n.ty == NodeTy::FUNC || n.ty == NodeTy::PROC
           || n.ty == NodeTy::EXTERN_FUNC || n.ty == NodeTy::EXTERN_PROC) {
      genFunc(_n);
    }
  }

  void genDeclList(unsigned int _declList) {
    Node declList = nodeManager->get(_declList);
    while (declList.ty == NodeTy::DECLLIST_CONS) {
      genDecl(declList.n1);
      declList = nodeManager->get(declList.n2);
    }
  }

};

#endif