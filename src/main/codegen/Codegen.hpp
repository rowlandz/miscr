#ifndef CODEGEN_CODEGEN
#define CODEGEN_CODEGEN

#include <llvm/IR/IRBuilder.h>
#include "common/ScopeStack.hpp"
#include "common/TypeContext.hpp"
#include "common/Ontology.hpp"

/// @brief LLVM IR code generation from AST.
class Codegen {
public:
  llvm::LLVMContext llvmctx;
  llvm::IRBuilder<>* b;
  llvm::Module* mod;
  const TypeContext& tyctx;
  const Ontology& ont;

  /// @brief Maps fully qualified names of `data` decls to their LLVM struct
  /// types.
  llvm::StringMap<llvm::StructType*> dataTypes;

  /// Maps variable names to their LLVM values
  ScopeStack<llvm::Value*> varValues;

  Codegen(const TypeContext& tyctx, const Ontology& ont)
      : tyctx(tyctx), ont(ont) {
    b = new llvm::IRBuilder<>(llvmctx);
    mod = new llvm::Module("MyModule", llvmctx);
    mod->setTargetTriple("x86_64-pc-linux-gnu");
    llvmctx.enableOpaquePointers();

    // Populates `dataTypes`
    for (llvm::StringRef typeName : ont.typeSpace.keys()) {
      std::vector<llvm::Type*> fieldTys;
      DataDecl* dataDecl = ont.getType(typeName);
      for (auto field : dataDecl->getFields()->asArrayRef())
        fieldTys.push_back(genType(field.second));
      auto st = llvm::StructType::get(llvmctx, fieldTys);
      st->setName(typeName);
      dataTypes[typeName] = st;
    }
  }

  ~Codegen() { delete mod; delete b; }
  Codegen(const Codegen&) = delete;
  Codegen& operator=(const Codegen&) = delete;

  llvm::Type* genType(TVar tvar) {
    Type ty = tyctx.resolve(tvar).second;
    if (ty.isNoType()) return b->getVoidTy();
    switch (ty.getID()) {
    case Type::ID::BOOL: return b->getInt1Ty();
    case Type::ID::i8: return b->getInt8Ty();
    case Type::ID::i16: return b->getInt16Ty();
    case Type::ID::i32: return b->getInt32Ty();
    case Type::ID::i64: return b->getInt64Ty();
    case Type::ID::f32: return b->getFloatTy();
    case Type::ID::f64: return b->getDoubleTy();
    case Type::ID::NAME: return dataTypes[ty.getName()->asStringRef()];
    case Type::ID::BREF:
    case Type::ID::OREF: return llvm::PointerType::get(llvmctx, 0);
    case Type::ID::UNIT: return b->getVoidTy();
    // NUMERIC just defaults to i32.
    case Type::ID::NUMERIC: return b->getInt32Ty();
    default:
      llvm_unreachable("Codegen::genType(TVar) case unimplemented");
    }
  }

  llvm::Type* genType(TypeExp* _texp) {
    if (auto texp = PrimitiveTypeExp::downcast(_texp)) {
      switch (texp->kind) {
      case PrimitiveTypeExp::BOOL:   return b->getInt1Ty();
      case PrimitiveTypeExp::f32:    return b->getFloatTy();
      case PrimitiveTypeExp::f64:    return b->getDoubleTy();
      case PrimitiveTypeExp::i8:     return b->getInt8Ty();
      case PrimitiveTypeExp::i16:    return b->getInt16Ty();
      case PrimitiveTypeExp::i32:    return b->getInt32Ty();
      case PrimitiveTypeExp::i64:    return b->getInt64Ty();
      case PrimitiveTypeExp::UNIT:   return b->getVoidTy();
      }
    }
    if (auto texp = NameTypeExp::downcast(_texp)) {
      return dataTypes[texp->getName()->asStringRef()];
    }
    if (auto texp = RefTypeExp::downcast(_texp)) {
      return llvm::PointerType::get(llvmctx, 0);
    }
    llvm_unreachable("genType(TypeExp*) case unimplemented");
  }

  /// Adds the parameters to the topmost scope in the scope stack. Also sets
  /// argument names in the LLVM IR.
  void addParamsToScope(llvm::Function* f, ParamList* paramList) {
    auto params = paramList->asArrayRef();
    unsigned int i = 0;
    for (llvm::Argument &arg : f->args()) {
      auto paramName = params[i].first->asStringRef();
      varValues.add(paramName, &arg);
      arg.setName(paramName);
      ++i;
    }
  }

  /// @brief Generates LLVM IR that performs the computation `_exp`.
  llvm::Value* genExp(Exp* _exp) {
    if (auto exp = BinopExp::downcast(_exp)) {
      llvm::Value* v1 = genExp(exp->getLHS());
      llvm::Value* v2 = genExp(exp->getRHS());
      switch (exp->getBinop()) {
      case BinopExp::ADD: return b->CreateAdd(v1, v2);
      case BinopExp::AND: return b->CreateAnd(v1, v2);
      case BinopExp::DIV: return b->CreateSDiv(v1, v2);
      case BinopExp::GE:  return b->CreateICmpSGE(v1, v2);
      case BinopExp::GT:  return b->CreateICmpSGT(v1, v2);
      case BinopExp::LE:  return b->CreateICmpSLE(v1, v2);
      case BinopExp::LT:  return b->CreateICmpSLT(v1, v2);
      case BinopExp::MOD: return b->CreateSRem(v1, v2);
      case BinopExp::MUL: return b->CreateMul(v1, v2);
      case BinopExp::OR:  return b->CreateOr(v1, v2);
      case BinopExp::SUB: return b->CreateSub(v1, v2);
      case BinopExp::EQ: {
        llvm::Type* operandType = v1->getType();
        if (operandType->isIntegerTy()) return b->CreateICmpEQ(v1, v2);
        if (operandType->isFloatingPointTy()) return b->CreateFCmpOEQ(v1, v2);
        llvm_unreachable("Unsupported type for == operator");
      }
      case BinopExp::NE: {
        llvm::Type* operandType = v1->getType();
        if (operandType->isIntegerTy()) return b->CreateICmpNE(v1, v2);
        if (operandType->isFloatingPointTy()) return b->CreateFCmpONE(v1, v2);
        llvm_unreachable("Unsupported type for /= operator");
      }
      default: llvm_unreachable("Unsupported binary operator");
      }
    }
    else if (auto e = AscripExp::downcast(_exp)) {
      return genExp(e->getAscriptee());
    }
    else if (auto e = BlockExp::downcast(_exp)) {
      llvm::ArrayRef<Exp*> stmtList = e->getStatements()->asArrayRef();
      llvm::Value* lastStmtVal = nullptr;
      for (Exp* stmt : stmtList) {
        lastStmtVal = genExp(stmt);
      }
      return lastStmtVal;
    }
    else if (auto e = BoolLit::downcast(_exp)) {
      return b->getInt1(e->getValue());
    }
    else if (auto e = BorrowExp::downcast(_exp)) {
      return genExp(e->getRefExp());
    }
    else if (auto e = CallExp::downcast(_exp)) {
      std::vector<llvm::Value*> args;
      for (Exp* arg : e->getArguments()->asArrayRef()) {
        args.push_back(genExp(arg));
      }

      if (e->isConstr()) {
        llvm::StructType* st = dataTypes[e->getFunction()->asStringRef()];
        llvm::Value* mem = b->CreateAlloca(st);
        unsigned int fieldIdx = 0;
        for (auto argV : args) {
          llvm::Value* fieldAddr = b->CreateGEP(st, mem,
            { b->getInt64(0), b->getInt32(fieldIdx) });
          b->CreateStore(argV, fieldAddr);
          ++fieldIdx;
        }
        return b->CreateLoad(st, mem);
      } else {
        llvm::StringRef funName = ont.mapName(e->getFunction()->asStringRef());
        llvm::Function* callee = mod->getFunction(funName);
        llvm::FunctionType* calleeType = callee->getFunctionType();
        return b->CreateCall(callee, args);
      }
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
      TVar ofTVar = tyctx.resolve(baseTVar).second.getInner();
      llvm::Type* ofTy = genType(ofTVar);
      return b->CreateGEP(ofTy, baseV, indexV);
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
    else if (auto e = MoveExp::downcast(_exp)) {
      llvm::Value* refExp = genExp(e->getRefExp());
      llvm::Type* tyToLoad = genType(e->getTVar());
      return b->CreateLoad(tyToLoad, refExp);
    }
    else if (auto e = ProjectExp::downcast(_exp)) {
      llvm::Value* baseV = genExp(e->getBase());
      DataDecl* typeDecl = ont.getType(e->getTypeName());
      llvm::StringRef fieldName = e->getFieldName()->asStringRef();
      unsigned int fieldIndex = 0;
      for (auto field : typeDecl->getFields()->asArrayRef()) {
        if (field.first->asStringRef() == fieldName) break;
        ++fieldIndex;
      }
      assert(fieldIndex < typeDecl->getFields()->asArrayRef().size());
      switch (e->getKind()) {
      case ProjectExp::DOT:
        return b->CreateExtractValue(baseV, fieldIndex);
      case ProjectExp::BRACKETS:
        return b->CreateGEP(dataTypes[e->getTypeName()], baseV,
          { b->getInt64(0), b->getInt32(fieldIndex) });
      case ProjectExp::ARROW:
        return b->CreateLoad(
          genType(e->getTVar()),
          b->CreateGEP(dataTypes[e->getTypeName()], baseV,
            { b->getInt64(0), b->getInt32(fieldIndex) })
        );
      }
    }
    else if (auto e = RefExp::downcast(_exp)) {
      llvm::Value* v = genExp(e->getInitializer());
      TVar initTy = e->getInitializer()->getTVar();
      llvm::AllocaInst* memCell = b->CreateAlloca(v->getType());
      b->CreateStore(v, memCell);
      return memCell;
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
    else if (auto e = UnopExp::downcast(_exp)) {
      llvm::Value* innerV = genExp(e->getInner());
      switch (e->getUnop()) {
      case UnopExp::NEG: return b->CreateNeg(innerV);
      case UnopExp::NOT: return b->CreateNot(innerV);
      }
    }
    else llvm_unreachable("genExp -- unsupported expression form");
  }

  /// Codegens a `func` or external function
  llvm::Function* genFunc(FunctionDecl* funDecl) {
    // Make the function type
    std::vector<llvm::Type*> paramTys;
    paramTys.reserve(funDecl->getParameters()->asArrayRef().size());
    for (auto param : funDecl->getParameters()->asArrayRef()) {
      paramTys.push_back(genType(param.second));
    }
    llvm::Type* retType = genType(funDecl->getReturnType());
    llvm::FunctionType* funcType =
      llvm::FunctionType::get(retType, paramTys, funDecl->isVariadic());

    llvm::Function* f = llvm::Function::Create(
      funcType,
      llvm::Function::ExternalLinkage,
      ont.mapName(funDecl->getName()->asStringRef()),
      mod
    );
    if (funDecl->hasBody()) {
      varValues.push();
      addParamsToScope(f, funDecl->getParameters());
      b->SetInsertPoint(llvm::BasicBlock::Create(llvmctx, "entry", f));
      llvm::Value* retVal = genExp(funDecl->getBody());
      // TODO: this is gross
      if (auto primRT = PrimitiveTypeExp::downcast(funDecl->getReturnType())) {
        if (primRT->kind == PrimitiveTypeExp::UNIT)
          b->CreateRetVoid();
        else
          b->CreateRet(retVal);
      } else {
        b->CreateRet(retVal);
      }
      varValues.pop();
    }
    return f;
  }

  void genModule(ModuleDecl* mod) {
    genDeclList(mod->getDecls());
  }

  void genDecl(Decl* decl) {
    if (auto mod = ModuleDecl::downcast(decl))
      genModule(mod);
    else if (auto func = FunctionDecl::downcast(decl))
      genFunc(func);
    else if (decl->id == AST::ID::DATA)
      {}
    else llvm_unreachable("Unsupported decl");
  }

  void genDeclList(DeclList* declList) {
    for (Decl* decl : declList->asArrayRef())
      genDecl(decl);
  }

};

#endif