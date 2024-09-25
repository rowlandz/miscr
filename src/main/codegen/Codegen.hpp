#ifndef CODEGEN_CODEGEN
#define CODEGEN_CODEGEN

#include <llvm/IR/IRBuilder.h>
#include "common/ScopeStack.hpp"
#include "common/TypeContext.hpp"
#include "common/Ontology.hpp"

class Codegen {
public:
  llvm::LLVMContext llvmctx;
  llvm::IRBuilder<>* b;
  llvm::Module* mod;
  const TypeContext* tyctx;
  const Ontology* ont;

  /** Maps variable names to their LLVM values */
  ScopeStack<llvm::Value*> varValues;

  Codegen(const TypeContext* tyctx, const Ontology* ont) {
    b = new llvm::IRBuilder<>(llvmctx);
    mod = new llvm::Module("MyModule", llvmctx);
    mod->setTargetTriple("x86_64-pc-linux-gnu");
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
      llvm::Value* sizeV = ty.getRuntimeArraySize() != nullptr ?
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

  llvm::Type* genType(TypeExp* _texp) {
    if (_texp->getID() == AST::ID::BOOL_TEXP) return b->getInt1Ty();
    if (_texp->getID() == AST::ID::f32_TEXP) return b->getFloatTy();
    if (_texp->getID() == AST::ID::f64_TEXP) return b->getDoubleTy();
    if (_texp->getID() == AST::ID::i8_TEXP) return b->getInt8Ty();
    if (_texp->getID() == AST::ID::i16_TEXP) return b->getInt16Ty();
    if (_texp->getID() == AST::ID::i32_TEXP) return b->getInt32Ty();
    if (_texp->getID() == AST::ID::i64_TEXP) return b->getInt64Ty();
    if (_texp->getID() == AST::ID::UNIT_TEXP) return b->getVoidTy();
    if (_texp->getID() == AST::ID::STR_TEXP) {
      return llvm::ArrayType::get(b->getInt8Ty(), 0);
    }
    if (auto texp = ArrayTypeExp::downcast(_texp)) {
      if (texp->getSize() == nullptr)
        return llvm::ArrayType::get(genType(texp->getInnerType()), 0);
      llvm::Value* sizeV = genExp(texp->getSize());
      llvm::ConstantInt* c = llvm::dyn_cast_or_null<llvm::ConstantInt>(sizeV);
      uint64_t arrSize = c ? c->getZExtValue() : 0;
      return llvm::ArrayType::get(genType(texp->getInnerType()), arrSize);
    }
    if (auto texp = RefTypeExp::downcast(_texp)) {
      llvm::Type* pointeeType = genType(texp->getPointeeType());
      return llvm::PointerType::get(pointeeType, 0);
    }
    llvm_unreachable("genType(TypeExp*) case unimplemented");
  }

  llvm::FunctionType* makeFunctionType(ParamList* paramList, TypeExp* retTy) {
    std::vector<llvm::Type*> paramTys;
    while (paramList->nonEmpty()) {
      paramTys.push_back(genType(paramList->getHeadParamType()));
      paramList = paramList->getTail();
    }
    llvm::Type* retType = genType(retTy);
    return llvm::FunctionType::get(retType, paramTys, false);
  }

  /// Adds the parameters to the topmost scope in the scope stack. Also sets
  /// argument names in the LLVM IR.
  void addParamsToScope(llvm::Function* f, ParamList* paramList) {
    for (llvm::Argument &arg : f->args()) {
      auto paramName = paramList->getHeadParamName()->asStringRef();
      varValues.add(paramName, &arg);
      arg.setName(paramName);
      paramList = paramList->getTail();
    }
  }

  /// @brief Generates LLVM IR that performs the computation `_exp`.
  llvm::Value* genExp(Exp* _exp) {
    AST::ID id = _exp->getID();

    if (auto exp = BinopExp::downcast(_exp)) {
      llvm::Value* v1 = genExp(exp->getLHS());
      llvm::Value* v2 = genExp(exp->getRHS());
      switch (id) {
      case AST::ID::ADD: return b->CreateAdd(v1, v2);
      case AST::ID::DIV: return b->CreateSDiv(v1, v2);
      case AST::ID::GE:  return b->CreateICmpSGE(v1, v2);
      case AST::ID::GT:  return b->CreateICmpSGT(v1, v2);
      case AST::ID::LE:  return b->CreateICmpSLE(v1, v2);
      case AST::ID::LT:  return b->CreateICmpSLT(v1, v2);
      case AST::ID::MUL: return b->CreateMul(v1, v2);
      case AST::ID::SUB: return b->CreateSub(v1, v2);
      case AST::ID::EQ: {
        llvm::Type* operandType = v1->getType();
        if (operandType->isIntegerTy()) return b->CreateICmpEQ(v1, v2);
        if (operandType->isFloatingPointTy()) return b->CreateFCmpOEQ(v1, v2);
        llvm_unreachable("Unsupported type for == operator");
      }
      case AST::ID::NE: {
        llvm::Type* operandType = v1->getType();
        if (operandType->isIntegerTy()) return b->CreateICmpNE(v1, v2);
        if (operandType->isFloatingPointTy()) return b->CreateFCmpONE(v1, v2);
        llvm_unreachable("Unsupported type for /= operator");
      }
      default: llvm_unreachable("Unsupported binary operator");
      }
    }
    else if (auto e = ArrayInitExp::downcast(_exp)) {
      auto arrTy = llvm::dyn_cast<llvm::ArrayType>(genType(e->getTVar()));
      llvm::Value* sizeV = genExp(e->getSize());
      auto sizeC = llvm::dyn_cast_or_null<llvm::ConstantInt>(genExp(e->getSize()));
      if (!sizeC) {
        llvm::errs() << "genExp does not support array initializer with "
                     << "non constant size\n";
        exit(1);
      }
      auto initC = llvm::dyn_cast_or_null<llvm::Constant>(genExp(e->getInitializer()));
      if (!initC) {
        llvm::errs() << "Array with non-constant initializer value!\n";
        exit(1);
      }
      std::vector<llvm::Constant*> vals(sizeC->getZExtValue(), initC);
      return llvm::ConstantArray::get(arrTy, vals);
    }
    else if (auto e = ArrayListExp::downcast(_exp)) {
      unsigned int arrSize = expListLength(e->getContent());
      TVar elemTy = tyctx->resolve(e->getTVar()).second.getInner();
      auto arrTy = llvm::ArrayType::get(genType(elemTy), arrSize);
      llvm::Value* ret = llvm::ConstantAggregateZero::get(arrTy);
      ExpList* expList = e->getContent();
      unsigned int idx = 0;
      while (expList->nonEmpty()) {
        llvm::Value* v = genExp(expList->getHead());
        ret = b->CreateInsertValue(ret, v, idx);
        ++idx;
        expList = expList->getTail();
      }
      return ret;
    }
    else if (auto e = AscripExp::downcast(_exp)) {
      return genExp(e->getAscriptee());
    }
    else if (auto e = BlockExp::downcast(_exp)) {
      ExpList* stmtList = e->getStatements();
      llvm::Value* lastStmtVal = nullptr;
      while (stmtList->nonEmpty()) {
        lastStmtVal = genExp(stmtList->getHead());
        stmtList = stmtList->getTail();
      }
      return lastStmtVal;
    }
    else if (auto e = CallExp::downcast(_exp)) {
      llvm::StringRef funName = ont->mapName(e->getFunction()->asStringRef());
      llvm::Function* callee = mod->getFunction(funName);
      llvm::FunctionType* calleeType = callee->getFunctionType();

      std::vector<llvm::Value*> args;
      ExpList* expList = e->getArguments();
      unsigned int paramIdx = 0;
      while (expList->nonEmpty()) {
        llvm::Value* arg = genExp(expList->getHead());
        arg = b->CreateBitCast(arg, calleeType->getParamType(paramIdx));
        args.push_back(arg);
        ++paramIdx;
        expList = expList->getTail();
      }
      return b->CreateCall(callee, args);
    }
    else if (auto e = DerefExp::downcast(_exp)) {
      llvm::Value* ofExp = genExp(e->getOf());
      llvm::Type* tyToLoad = genType(e->getTVar());
      return b->CreateLoad(tyToLoad, ofExp);
    }
    else if (auto e = NameExp::downcast(_exp)) {
      Name* name = e->getName();
      return varValues.getOrElse(name->asStringRef(), nullptr);
    }
    else if (auto e = IfExp::downcast(_exp)) {
      llvm::Function* f = b->GetInsertBlock()->getParent();
      llvm::Value* condition = genExp(e->getCondExp());
      auto thenBlock = llvm::BasicBlock::Create(llvmctx, "then");
      auto elseBlock = llvm::BasicBlock::Create(llvmctx, "else");
      auto contBlock = llvm::BasicBlock::Create(llvmctx, "ifcont");
      b->CreateCondBr(condition, thenBlock, elseBlock);

      f->getBasicBlockList().push_back(thenBlock);
      b->SetInsertPoint(thenBlock);
      llvm::Value* thenResult = genExp(e->getThenExp());
      b->CreateBr(contBlock);
      thenBlock = b->GetInsertBlock();

      f->getBasicBlockList().push_back(elseBlock);
      b->SetInsertPoint(elseBlock);
      llvm::Value* elseResult = genExp(e->getElseExp());
      b->CreateBr(contBlock);
      elseBlock = b->GetInsertBlock();

      f->getBasicBlockList().push_back(contBlock);
      b->SetInsertPoint(contBlock);
      llvm::PHINode* phiNode = b->CreatePHI(genType(e->getTVar()), 2);
      phiNode->addIncoming(thenResult, thenBlock);
      phiNode->addIncoming(elseResult, elseBlock);
      return phiNode;
    }
    else if (auto e = IndexExp::downcast(_exp)) {
      llvm::Value* baseV = genExp(e->getBase());
      llvm::Value* indexV = genExp(e->getIndex());
      TVar baseTVar = e->getBase()->getTVar();
      TVar arrTVar = tyctx->resolve(baseTVar).second.getInner();
      auto arrTy = llvm::dyn_cast_or_null<llvm::ArrayType>(genType(arrTVar));
      return b->CreateGEP(arrTy, baseV, { b->getInt64(0), indexV });
    }
    else if (auto e = IntLit::downcast(_exp)) {
      return llvm::ConstantInt::get(genType(e->getTVar()), e->asLong());
    }
    else if (auto e = LetExp::downcast(_exp)) {
      llvm::Value* v = genExp(e->getDefinition());
      llvm::StringRef boundIdentName = e->getBoundIdent()->asStringRef();
      v->setName(boundIdentName);
      varValues.add(boundIdentName, v);
      return nullptr;
    }
    else if (auto e = RefExp::downcast(_exp)) {
      AST::ID initID = e->getInitializer()->getID();
      if (initID == AST::ID::ARRAY_INIT || initID == AST::ID::ARRAY_LIST) {
        return genRefToExp(e->getInitializer());
      } else {
        llvm::Value* v = genExp(e->getInitializer());
        TVar initTy = e->getInitializer()->getTVar();
        llvm::AllocaInst* memCell = b->CreateAlloca(v->getType());
        b->CreateStore(v, memCell);
        return memCell;
      }
    }
    else if (auto e = StoreExp::downcast(_exp)) {
      llvm::Value* lhs = genExp(e->getLHS());
      llvm::Value* rhs = genExp(e->getRHS());
      b->CreateStore(rhs, lhs);
      return rhs;
    }
    else if (auto e = StringLit::downcast(_exp)) {
      return b->CreateGlobalString(e->processEscapes());
    }
    else llvm_unreachable("genExp -- unsupported expression form");
  }

  /// @brief Generates LLVM IR that performs `_exp` and stores the result on
  /// the stack. The returned value is a pointer to the result. In other words,
  /// this is like calling `genExp(&(_exp))`. Certain expressions (like
  /// constructing arrays of unknown size) can only be done on the stack and
  /// not using pure LLVM values.
  llvm::Value* genRefToExp(Exp* _exp) {

    // TODO: skipping initializer for now.
    if (auto e = ArrayInitExp::downcast(_exp)) {
      auto arrTy = llvm::dyn_cast<llvm::ArrayType>(genType(e->getTVar()));
      llvm::Value* arrSize = genExp(e->getSize());
      llvm::Value* ret = b->CreateAlloca(arrTy->getElementType(), arrSize);
      return b->CreateBitCast(ret, llvm::PointerType::get(arrTy, 0));
    }

    else if (auto e = ArrayListExp::downcast(_exp)) {
      auto arrTy = llvm::dyn_cast<llvm::ArrayType>(genType(e->getTVar()));
      auto elemTy = arrTy->getArrayElementType();
      unsigned int arrayLen = expListLength(e->getContent());
      llvm::Value* ret = b->CreateAlloca(elemTy, b->getInt32(arrayLen));
      ExpList* expList = e->getContent();
      unsigned int idx = 0;
      while (expList->nonEmpty()) {
        llvm::Value* elem = genExp(expList->getHead());
        llvm::Value* ptr = b->CreateGEP(elemTy, ret, b->getInt32(idx) );
        b->CreateStore(elem, ptr);
        ++idx;
        expList = expList->getTail();
      }
      // return ret;
      arrTy = llvm::ArrayType::get(elemTy, arrayLen);
      return b->CreateBitCast(ret, llvm::PointerType::get(arrTy, 0));
    }

    else {
      llvm::errs() << "genRefToExp cannot handle AST::ID::";
      llvm::errs() << ASTIDToString(_exp->getID()) << "\n";
      exit(1);
    }
  }

  unsigned int expListLength(ExpList* expList) {
    unsigned int ret = 0;
    while (expList->nonEmpty()) {
      ++ret;
      expList = expList->getTail();
    }
    return ret;
  }

  /** Codegens a `func`, `extern func`. */
  llvm::Function* genFunc(FunctionDecl* funDecl) {

    llvm::StringRef mappedName = ont->mapName(funDecl->getName()->asStringRef());

    llvm::Function* f = llvm::Function::Create(
      makeFunctionType(funDecl->getParameters(), funDecl->getReturnType()),
      llvm::Function::ExternalLinkage,
      mappedName,
      mod
    );

    if (funDecl->getID() == AST::ID::FUNC) {
      varValues.push();
      addParamsToScope(f, funDecl->getParameters());
      b->SetInsertPoint(llvm::BasicBlock::Create(llvmctx, "entry", f));
      llvm::Value* retVal = genExp(funDecl->getBody());
      b->CreateRet(retVal);
      varValues.pop();
    }

    return f;
  }

  void genModule(ModuleDecl* mod) {
    genDeclList(mod->getDecls());
  }

  void genDecl(Decl* decl) {
    if (auto mod = ModuleDecl::downcast(decl)) {
      genModule(mod);
    }
    else if (auto func = FunctionDecl::downcast(decl)) {
      genFunc(func);
    }
    else llvm_unreachable("Unsupported decl");
  }

  void genDeclList(DeclList* declList) {
    while (declList->nonEmpty()) {
      genDecl(declList->getHead());
      declList = declList->getTail();
    }
  }

};

#endif