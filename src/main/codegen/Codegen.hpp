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
    case Type::ID::ARRAY_SART: {
      llvm::Value* sizeV = ty.getRuntimeArraySize().exists() ?
                           genExp(ty.getRuntimeArraySize()) :
                           b->getInt32(0);
      llvm::ConstantInt* c = llvm::dyn_cast_or_null<llvm::ConstantInt>(sizeV);
      uint64_t arrSize = c ? c->getZExtValue() : 0;
      return llvm::ArrayType::get(genType(ty.getInner()), arrSize);
    }
    case Type::ID::ARRAY_SACT: {
      unsigned int arrSize = ty.getCompileTimeArraySize();
      return llvm::ArrayType::get(genType(ty.getInner()), arrSize);
    }
    case Type::ID::BOOL: return b->getInt1Ty();
    case Type::ID::i8: return b->getInt8Ty();
    case Type::ID::i16: return b->getInt16Ty();
    case Type::ID::i32: return b->getInt32Ty();
    case Type::ID::i64: return b->getInt64Ty();
    case Type::ID::f32: return b->getFloatTy();
    case Type::ID::f64: return b->getDoubleTy();
    case Type::ID::RREF:
    case Type::ID::WREF: {
      llvm::Type* pointeeType = genType(ty.getInner());
      return llvm::PointerType::get(pointeeType, 0);
    }
    case Type::ID::UNIT: return b->getVoidTy();

    // NUMERIC just defaults to i32.
    case Type::ID::NUMERIC: return b->getInt32Ty();
    default:
      llvm::errs() << "genType(TVar) case unimplemented\n";
      exit(1);
    }
  }

  llvm::Type* genType(Addr<TypeExp> _texp) {
    switch(astctx->get(_texp).getID()) {
    case AST::ID::i8_TEXP: return b->getInt8Ty();
    case AST::ID::i16_TEXP: return b->getInt16Ty();
    case AST::ID::i32_TEXP: return b->getInt32Ty();
    case AST::ID::i64_TEXP: return b->getInt64Ty();
    case AST::ID::f32_TEXP: return b->getFloatTy();
    case AST::ID::f64_TEXP: return b->getDoubleTy();
    case AST::ID::ARRAY_TEXP: {
      ArrayTypeExp texp = astctx->GET_UNSAFE<ArrayTypeExp>(_texp);
      if (texp.getSize().isError())
        return llvm::ArrayType::get(genType(texp.getInnerType()), 0);
      llvm::Value* sizeV = genExp(texp.getSize());
      llvm::ConstantInt* c = llvm::dyn_cast_or_null<llvm::ConstantInt>(sizeV);
      uint64_t arrSize = c ? c->getZExtValue() : 0;
      return llvm::ArrayType::get(genType(texp.getInnerType()), arrSize);
    }
    case AST::ID::BOOL_TEXP: return b->getInt1Ty();
    case AST::ID::REF_TEXP:
    case AST::ID::WREF_TEXP: {
      RefTypeExp texp = astctx->GET_UNSAFE<RefTypeExp>(_texp);
      llvm::Type* pointeeType = genType(texp.getPointeeType());
      return llvm::PointerType::get(pointeeType, 0);
    }
    case AST::ID::STR_TEXP: return llvm::ArrayType::get(b->getInt8Ty(), 0);
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

  /// @brief Generates LLVM IR that performs the computation `_exp`.
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
      case AST::ID::GE:  return b->CreateICmpSGE(v1, v2);
      case AST::ID::GT:  return b->CreateICmpSGT(v1, v2);
      case AST::ID::LE:  return b->CreateICmpSLE(v1, v2);
      case AST::ID::LT:  return b->CreateICmpSLT(v1, v2);
      case AST::ID::MUL: return b->CreateMul(v1, v2);
      case AST::ID::SUB: return b->CreateSub(v1, v2);
      default: assert(false && "unimplemented");
      }
    }

    else if (id == AST::ID::ARRAY_INIT) {
      ArrayInitExp e = astctx->GET_UNSAFE<ArrayInitExp>(_exp);
      auto arrTy = llvm::dyn_cast<llvm::ArrayType>(genType(e.getTVar()));
      llvm::Value* sizeV = genExp(e.getSize());
      auto sizeC = llvm::dyn_cast_or_null<llvm::ConstantInt>(genExp(e.getSize()));
      if (!sizeC) {
        llvm::errs() << "genExp does not support array initializer with "
                     << "non constant size\n";
        exit(1);
      }
      auto initC = llvm::dyn_cast_or_null<llvm::Constant>(genExp(e.getInitializer()));
      if (!initC) {
        llvm::errs() << "Array with non-constant initializer value!\n";
        exit(1);
      }
      std::vector<llvm::Constant*> vals(sizeC->getZExtValue(), initC);
      return llvm::ConstantArray::get(arrTy, vals);
    }

    else if (id == AST::ID::ARRAY_LIST) {
      ArrayListExp e = astctx->GET_UNSAFE<ArrayListExp>(_exp);
      unsigned int arrSize = expListLength(e.getContent());
      TVar elemTy = tyctx->resolve(e.getTVar()).second.getInner();
      auto arrTy = llvm::ArrayType::get(genType(elemTy), arrSize);
      llvm::Value* ret = llvm::ConstantAggregateZero::get(arrTy);
      ExpList expList = astctx->get(e.getContent());
      unsigned int idx = 0;
      while (expList.nonEmpty()) {
        llvm::Value* v = genExp(expList.getHead());
        ret = b->CreateInsertValue(ret, v, idx);
        ++idx;
        expList = astctx->get(expList.getTail());
      }
      return ret;
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
      llvm::FunctionType* calleeType = callee->getFunctionType();

      std::vector<llvm::Value*> args;
      ExpList expList = astctx->get(e.getArguments());
      unsigned int paramIdx = 0;
      while (expList.nonEmpty()) {
        llvm::Value* arg = genExp(expList.getHead());
        arg = b->CreateBitCast(arg, calleeType->getParamType(paramIdx));
        args.push_back(arg);
        ++paramIdx;
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

    else if (id == AST::ID::INDEX) {
      IndexExp e = astctx->GET_UNSAFE<IndexExp>(_exp);
      llvm::Value* baseV = genExp(e.getBase());
      llvm::Value* indexV = genExp(e.getIndex());
      TVar baseTVar = astctx->get(e.getBase()).getTVar();
      TVar arrTVar = tyctx->resolve(baseTVar).second.getInner();
      auto arrTy = llvm::dyn_cast_or_null<llvm::ArrayType>(genType(arrTVar));
      return b->CreateGEP(arrTy, baseV, { b->getInt64(0), indexV });
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

    else if (id == AST::ID::NE) {
      BinopExp e = astctx->GET_UNSAFE<BinopExp>(_exp);
      llvm::Value* v1 = genExp(e.getLHS());
      llvm::Value* v2 = genExp(e.getRHS());

      llvm::Type* operandType = v1->getType();
      if (operandType->isIntegerTy())
        return b->CreateICmpNE(v1, v2);
      else if (operandType->isFloatingPointTy())
        return b->CreateFCmpONE(v1, v2);
      else {
        llvm::errs() << "Fatal error: Unexpected type for NE\n";
        exit(1);
      }
    }

    else if (id == AST::ID::REF_EXP || id == AST::ID::WREF_EXP) {
      RefExp e = astctx->GET_UNSAFE<RefExp>(_exp);
      AST::ID initID = astctx->get(e.getInitializer()).getID();
      if (initID == AST::ID::ARRAY_INIT || initID == AST::ID::ARRAY_LIST) {
        return genRefToExp(e.getInitializer());
      } else {
        llvm::Value* v = genExp(e.getInitializer());
        TVar initTy = astctx->get(e.getInitializer()).getTVar();
        llvm::AllocaInst* memCell = b->CreateAlloca(v->getType());
        b->CreateStore(v, memCell);
        return memCell;
      }
    }

    else if (id == AST::ID::STORE) {
      StoreExp e = astctx->GET_UNSAFE<StoreExp>(_exp);
      llvm::Value* lhs = genExp(e.getLHS());
      llvm::Value* rhs = genExp(e.getRHS());
      b->CreateStore(rhs, lhs);
      return rhs;
    }

    else if (id == AST::ID::STRING_LIT) {
      StringLit e = astctx->GET_UNSAFE<StringLit>(_exp);
      return b->CreateGlobalString(e.processEscapes());
    }

    else {
      llvm::errs() << "genExp cannot handle AST::ID::"
                   << ASTIDToString(id) << "\n";
      exit(1);
    }
  }

  /// @brief Generates LLVM IR that performs `_exp` and stores the result on
  /// the stack. The returned value is a pointer to the result. In other words,
  /// this is like calling `genExp(&(_exp))`. Certain expressions (like
  /// constructing arrays of unknown size) can only be done on the stack and
  /// not using pure LLVM values.
  llvm::Value* genRefToExp(Addr<Exp> _exp) {
    Exp exp = astctx->get(_exp);
    AST::ID id = exp.getID();

    // TODO: skipping initializer for now.
    if (id == AST::ID::ARRAY_INIT) {
      ArrayInitExp e = astctx->GET_UNSAFE<ArrayInitExp>(_exp);
      auto arrTy = llvm::dyn_cast<llvm::ArrayType>(genType(e.getTVar()));
      llvm::Value* arrSize = genExp(e.getSize());
      llvm::Value* ret = b->CreateAlloca(arrTy->getElementType(), arrSize);
      return b->CreateBitCast(ret, llvm::PointerType::get(arrTy, 0));
    }

    else if (id == AST::ID::ARRAY_LIST) {
      ArrayListExp e = astctx->GET_UNSAFE<ArrayListExp>(_exp);
      auto arrTy = llvm::dyn_cast<llvm::ArrayType>(genType(e.getTVar()));
      auto elemTy = arrTy->getArrayElementType();
      unsigned int arrayLen = expListLength(e.getContent());
      llvm::Value* ret = b->CreateAlloca(elemTy, b->getInt32(arrayLen));
      ExpList expList = astctx->get(e.getContent());
      unsigned int idx = 0;
      while (expList.nonEmpty()) {
        llvm::Value* elem = genExp(expList.getHead());
        llvm::Value* ptr = b->CreateGEP(elemTy, ret, b->getInt32(idx) );
        b->CreateStore(elem, ptr);
        ++idx;
        expList = astctx->get(expList.getTail());
      }
      // return ret;
      arrTy = llvm::ArrayType::get(elemTy, arrayLen);
      return b->CreateBitCast(ret, llvm::PointerType::get(arrTy, 0));
    }

    else {
      llvm::errs() << "genRefToExp cannot handle AST::ID::";
      llvm::errs() << ASTIDToString(id) << "\n";
      exit(1);
    }
  }

  unsigned int expListLength(Addr<ExpList> _expList) {
    ExpList expList = astctx->get(_expList);
    unsigned int ret = 0;
    while (expList.nonEmpty()) {
      ++ret;
      expList = astctx->get(expList.getTail());
    }
    return ret;
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