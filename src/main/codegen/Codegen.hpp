#ifndef CODEGEN_CODEGEN
#define CODEGEN_CODEGEN

#include <llvm/IR/IRBuilder.h>
#include "common/ScopeStack.hpp"
#include "common/ASTContext.hpp"
#include "common/TypeContext.hpp"
#include "common/Ontology.hpp"

class Codegen {
public:
  llvm::LLVMContext llvmctx;
  llvm::IRBuilder<>* b;
  llvm::Module* mod;
  const ASTContext* astctx;
  const TypeContext* tyctx;
  const Ontology* ont;

  /** Maps variable names to their LLVM values */
  ScopeStack<llvm::Value*> varValues;

  Codegen(const ASTContext* astctx, const TypeContext* tyctx,
        const Ontology* ont) {
    b = new llvm::IRBuilder<>(llvmctx);
    mod = new llvm::Module("MyModule", llvmctx);
    mod->setTargetTriple("x86_64-pc-linux-gnu");
    this->astctx = astctx;
    this->tyctx = tyctx;
    this->ont = ont;
  }

  ~Codegen() {
    delete mod;
    delete b;
  }

  llvm::Type* genType(TVar tvar) {
    Type ty = tyctx->resolve(tvar).second;
    if (ty.isNoType()) return b->getVoidTy();
    switch (ty.getID()) {
    case Type::ID::BOOL: return b->getInt1Ty();
    case Type::ID::i8: return b->getInt8Ty();
    case Type::ID::i32: return b->getInt32Ty();
    case Type::ID::f32: return b->getFloatTy();
    case Type::ID::f64: return b->getDoubleTy();
    case Type::ID::RREF:
    case Type::ID::WREF: return llvm::PointerType::get(llvmctx, 0);
    case Type::ID::UNIT: return b->getVoidTy();
    default:
      llvm::errs() << "genType(TVar) case unimplemented\n";
      exit(1);
    }
  }

  llvm::Type* genType(Addr<TypeExp> _texp) {
    switch(astctx->get(_texp).getID()) {
    case AST::ID::i8_TEXP: return b->getInt8Ty();
    case AST::ID::i32_TEXP: return b->getInt32Ty();
    case AST::ID::f32_TEXP: return b->getFloatTy();
    case AST::ID::f64_TEXP: return b->getDoubleTy();
    case AST::ID::ARRAY_TEXP: return llvm::PointerType::get(llvmctx, 0);
    case AST::ID::BOOL_TEXP: return b->getInt1Ty();
    case AST::ID::REF_TEXP:
    case AST::ID::WREF_TEXP: return llvm::PointerType::get(llvmctx, 0);
    case AST::ID::UNIT_TEXP: return b->getVoidTy();
    default:
      llvm::errs() << "genType(Addr<TypeExp>) case unimplemented\n";
      exit(1);
    }
  }

  llvm::FunctionType* makeFunctionType(Addr<ParamList> _paramList,
        Addr<TypeExp> _retTy) {
    ParamList paramList = astctx->get(_paramList);
    std::vector<llvm::Type*> paramTys;
    while (paramList.nonEmpty()) {
      paramTys.push_back(genType(paramList.getHeadParamType()));
      paramList = astctx->get(paramList.getTail());
    }
    llvm::Type* retTy = genType(_retTy);
    return llvm::FunctionType::get(retTy, paramTys, false);
  }

  /// Adds the parameters to the topmost scope in the scope stack. Also sets
  /// argument names in the LLVM IR.
  void addParamsToScope(llvm::Function* f, Addr<ParamList> _paramList) {
    ParamList paramList = astctx->get(_paramList);
    for (llvm::Argument &arg : f->args()) {
      auto paramName = astctx->get(paramList.getHeadParamName()).asString();
      varValues.add(paramName, &arg);
      arg.setName(paramName);
      paramList = astctx->get(paramList.getTail());
    }
  }

  llvm::Value* genExp(Addr<Exp> _exp) {
    Exp exp = astctx->get(_exp);
    AST::ID id = exp.getID();

    if (exp.isBinopExp() && id != AST::ID::EQ && id != AST::ID::NE) {
      BinopExp e = astctx->GET_UNSAFE<BinopExp>(_exp);
      llvm::Value* v1 = genExp(e.getLHS());
      llvm::Value* v2 = genExp(e.getRHS());
      switch (id) {
      case AST::ID::ADD: return b->CreateAdd(v1, v2);
      case AST::ID::DIV: return b->CreateSDiv(v1, v2);
      case AST::ID::MUL: return b->CreateMul(v1, v2);
      case AST::ID::SUB: return b->CreateSub(v1, v2);
      default: assert(false && "unimplemented");
      }
    }

    else if (id == AST::ID::ASCRIP) {
      AscripExp e = astctx->GET_UNSAFE<AscripExp>(_exp);
      return genExp(e.getAscriptee());
    }

    else if (id == AST::ID::BLOCK) {
      BlockExp e = astctx->GET_UNSAFE<BlockExp>(_exp);
      ExpList stmtList = astctx->get(e.getStatements());
      llvm::Value* lastStmtVal = nullptr;
      while (stmtList.nonEmpty()) {
        lastStmtVal = genExp(stmtList.getHead());
        stmtList = astctx->get(stmtList.getTail());
      }
      return lastStmtVal;
    }

    else if (id == AST::ID::CALL) {
      CallExp e = astctx->GET_UNSAFE<CallExp>(_exp);
      FQIdent fqIdent = astctx->GET_UNSAFE<FQIdent>(e.getFunction());
      std::string name = ont->getName(fqIdent.getKey());
      llvm::Function* callee = mod->getFunction(name);

      std::vector<llvm::Value*> args;
      ExpList expList = astctx->get(e.getArguments());
      while (expList.nonEmpty()) {
        args.push_back(genExp(expList.getHead()));
        expList = astctx->get(expList.getTail());
      }
      return b->CreateCall(callee, args);
    }

    else if (id == AST::ID::DEREF) {
      DerefExp e = astctx->GET_UNSAFE<DerefExp>(_exp);
      llvm::Value* ofExp = genExp(e.getOf());
      llvm::Type* tyToLoad = genType(e.getTVar());
      return b->CreateLoad(tyToLoad, ofExp);
    }

    else if (id == AST::ID::EQ) {
      BinopExp e = astctx->GET_UNSAFE<BinopExp>(_exp);
      llvm::Value* v1 = genExp(e.getLHS());
      llvm::Value* v2 = genExp(e.getRHS());

      llvm::Type* operandType = v1->getType();
      if (operandType->isIntegerTy())
        return b->CreateICmpEQ(v1, v2);
      else if (operandType->isFloatingPointTy())
        return b->CreateFCmpOEQ(v1, v2);
      else {
        llvm::errs() << "Fatal error: Unexpected type for EQ\n";
        exit(1);
      }
    }

    else if (id == AST::ID::ENAME) {
      NameExp e = astctx->GET_UNSAFE<NameExp>(_exp);
      Name name = astctx->get(e.getName());
      if (name.isUnqualified()) {
        std::string ident = astctx->GET_UNSAFE<Ident>(e.getName()).asString();
        return varValues.getOrElse(ident, nullptr);
      } else {
        llvm::errs() << "Didn't expect qualified identifier\n";
        exit(1);
      }
    }

    else if (id == AST::ID::IF) {
      IfExp e = astctx->GET_UNSAFE<IfExp>(_exp);
      llvm::Function* f = b->GetInsertBlock()->getParent();
      llvm::Value* condition = genExp(e.getCondExp());
      auto thenBlock = llvm::BasicBlock::Create(llvmctx, "then");
      auto elseBlock = llvm::BasicBlock::Create(llvmctx, "else");
      auto contBlock = llvm::BasicBlock::Create(llvmctx, "ifcont");
      b->CreateCondBr(condition, thenBlock, elseBlock);

      f->getBasicBlockList().push_back(thenBlock);
      b->SetInsertPoint(thenBlock);
      llvm::Value* thenResult = genExp(e.getThenExp());
      b->CreateBr(contBlock);
      thenBlock = b->GetInsertBlock();

      f->getBasicBlockList().push_back(elseBlock);
      b->SetInsertPoint(elseBlock);
      llvm::Value* elseResult = genExp(e.getElseExp());
      b->CreateBr(contBlock);
      elseBlock = b->GetInsertBlock();

      f->getBasicBlockList().push_back(contBlock);
      b->SetInsertPoint(contBlock);
      llvm::PHINode* phiNode = b->CreatePHI(genType(e.getTVar()), 2);
      phiNode->addIncoming(thenResult, thenBlock);
      phiNode->addIncoming(elseResult, elseBlock);
      return phiNode;
    }

    else if (id == AST::ID::INT_LIT) {
      IntLit lit = astctx->GET_UNSAFE<IntLit>(_exp);
      return llvm::ConstantInt::get(genType(lit.getTVar()), lit.asLong());
    }

    else if (id == AST::ID::LET) {
      LetExp e = astctx->GET_UNSAFE<LetExp>(_exp);
      llvm::Value* v = genExp(e.getDefinition());
      std::string boundIdentName = astctx->get(e.getBoundIdent()).asString();
      v->setName(boundIdentName);
      varValues.add(boundIdentName, v);
      return nullptr;
    }

    else if (id == AST::ID::REF_EXP || id == AST::ID::WREF_EXP) {
      RefExp e = astctx->GET_UNSAFE<RefExp>(_exp);
      llvm::Value* v = genExp(e.getInitializer());
      TVar initTy = astctx->get(e.getInitializer()).getTVar();
      llvm::AllocaInst* memCell = b->CreateAlloca(v->getType());
      b->CreateStore(v, memCell);
      return memCell;
    }

    else if (id == AST::ID::STORE) {
      StoreExp e = astctx->GET_UNSAFE<StoreExp>(_exp);
      llvm::Value* lhs = genExp(e.getLHS());
      llvm::Value* rhs = genExp(e.getRHS());
      b->CreateStore(rhs, lhs);
      return rhs;
    }

    else {
      llvm::errs() << "genExp cannot handle AST::ID::";
      llvm::errs() << ASTIDToString(id) << "\n";
      exit(1);
    }
  }

  /** Codegens a `func`, `extern func`. */
  llvm::Function* genFunc(Addr<FunctionDecl> _funDecl) {
    FunctionDecl funDecl = astctx->get(_funDecl);
    
    FQIdent fqName = astctx->GET_UNSAFE<FQIdent>(funDecl.getName());

    llvm::Function* f = llvm::Function::Create(
      makeFunctionType(funDecl.getParameters(), funDecl.getReturnType()),
      llvm::Function::ExternalLinkage,
      ont->getName(fqName.getKey()),
      mod
    );

    if (funDecl.getID() == AST::ID::FUNC) {
      varValues.push();
      addParamsToScope(f, funDecl.getParameters());
      b->SetInsertPoint(llvm::BasicBlock::Create(llvmctx, "entry", f));
      llvm::Value* retVal = genExp(funDecl.getBody());
      b->CreateRet(retVal);
      varValues.pop();
    }

    return f;
  }

  void genModule(Addr<ModuleDecl> _mod) {
    ModuleDecl mod = astctx->get(_mod);
    genDeclList(mod.getDecls());
  }

  void genDecl(Addr<Decl> _decl) {
    switch (astctx->get(_decl).getID()) {
    case AST::ID::MODULE:
      genModule(_decl.UNSAFE_CAST<ModuleDecl>());
      break;
    case AST::ID::FUNC: case AST::ID::EXTERN_FUNC:
      genFunc(_decl.UNSAFE_CAST<FunctionDecl>());
      break;
    default:
      llvm::errs() << "genDecl: unexpected AST::ID::" <<
        ASTIDToString(astctx->get(_decl).getID()) << "\n";
    }
  }

  void genDeclList(Addr<DeclList> _declList) {
    DeclList declList = astctx->get(_declList);
    while (declList.nonEmpty()) {
      genDecl(declList.getHead());
      declList = astctx->get(declList.getTail());
    }
  }

};

#endif