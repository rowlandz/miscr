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
  const Ontology& ont;

  /// @brief Maps fully qualified names of structs to their LLVM struct types.
  llvm::StringMap<llvm::StructType*> structTypes;

  /// @brief Maps local variable names to their (stack) addresses.
  ScopeStack<llvm::AllocaInst*> varAddresses;

  Codegen(const Ontology& ont) : ont(ont) {
    b = new llvm::IRBuilder<>(llvmctx);
    mod = new llvm::Module("MyModule", llvmctx);
    mod->setTargetTriple("x86_64-pc-linux-gnu");

    // Populates `structTypes`
    for (llvm::StringRef typeName : ont.typeSpace.keys()) {
      std::vector<llvm::Type*> fieldTys;
      StructDecl* structDecl = ont.getType(typeName);
      for (auto field : structDecl->getFields()->asArrayRef())
        fieldTys.push_back(genType(field.second));
      auto st = llvm::StructType::get(llvmctx, fieldTys);
      st->setName(typeName);
      structTypes[typeName] = st;
    }
  }

  ~Codegen() { delete mod; delete b; }
  Codegen(const Codegen&) = delete;
  Codegen& operator=(const Codegen&) = delete;

  llvm::Type* genType(Type* ty) {
    if (auto constraint = Constraint::downcast(ty)) {
      switch (constraint->kind) {
      case Constraint::DECIMAL:   return b->getDoubleTy();
      case Constraint::NUMERIC:   return b->getInt32Ty();
      }
    }
    if (auto nameTy = NameType::downcast(ty)) {
      return structTypes[nameTy->asString];
    }
    if (auto primTy = PrimitiveType::downcast(ty)) {
      switch (primTy->kind) {
      case PrimitiveType::BOOL:   return b->getInt1Ty();
      case PrimitiveType::f32:    return b->getFloatTy();
      case PrimitiveType::f64:    return b->getDoubleTy();
      case PrimitiveType::i8:     return b->getInt8Ty();
      case PrimitiveType::i16:    return b->getInt16Ty();
      case PrimitiveType::i32:    return b->getInt32Ty();
      case PrimitiveType::i64:    return b->getInt64Ty();
      case PrimitiveType::UNIT:   return b->getVoidTy();
      }
    }
    if (auto refTy = RefType::downcast(ty)) {
      return llvm::PointerType::get(llvmctx, 0);
    }
    if (auto tyVar = TypeVar::downcast(ty)) {
      llvm_unreachable("TypeVars should not appear after Resolver");
    }
    llvm_unreachable("Codegen::genType(TVar) case unimplemented");
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
      return structTypes[texp->getName()->asStringRef()];
    }
    if (auto texp = RefTypeExp::downcast(_texp)) {
      return llvm::PointerType::get(llvmctx, 0);
    }
    llvm_unreachable("genType(TypeExp*) case unimplemented");
  }

  /// @brief Initializes stack memory for function arguments. The insertion
  /// point of `b` must be the beginning of @p f.
  /// @param f
  /// @param params
  void initializeFunctionArguments(llvm::Function* f, ParamList* paramList) {
    auto params = paramList->asArrayRef();
    unsigned int i = 0;
    for (llvm::Argument &arg : f->args()) {
      llvm::StringRef paramName = params[i].first->asStringRef();
      llvm::AllocaInst* memCell = b->CreateAlloca(arg.getType());
      b->CreateStore(&arg, memCell);
      varAddresses.add(paramName, memCell);
      memCell->setName(paramName);
      ++i;
    }
  }

  /// @brief Generates LLVM IR that computes @p lvalue. Returns the _address_
  /// of the computed value rather than the value itself.
  llvm::Value* genExpByReference(Exp* lvalue) {
    assert(lvalue->isLvalue() && "expected lvalue");
    if (auto e = DerefExp::downcast(lvalue)) {
      return genExp(e->getOf());
    }
    if (auto e = NameExp::downcast(lvalue)) {
      llvm::AllocaInst* variableAddress =
        varAddresses.getOrElse(e->getName()->asStringRef(), nullptr);
      assert(variableAddress && "variable not found");
      return variableAddress;
    }
    if (auto e = ProjectExp::downcast(lvalue)) {
      if (e->getKind() == ProjectExp::ARROW) {
        return genProjectExp(e->getBase(), e->getFieldName(),
          ProjectExp::Kind::BRACKETS, e->getTypeName());
      }
    }
    llvm_unreachable("Codegen::genExpByReference() fallthrough");
    return nullptr;
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
    else if (auto e = AddrOfExp::downcast(_exp)) {
      return genExpByReference(e->getOf());
    }
    else if (auto e = AscripExp::downcast(_exp)) {
      return genExp(e->getAscriptee());
    }
    else if (auto e = AssignExp::downcast(_exp)) {
      llvm::Value* lhsAddr = genExpByReference(e->getLHS());
      llvm::Value* rhs = genExp(e->getRHS());
      b->CreateStore(rhs, lhsAddr);
      return nullptr;
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
      for (Exp* arg : e->getArguments()->asArrayRef())
        { args.push_back(genExp(arg)); }

      llvm::StringRef funName = ont.mapName(e->getFunction()->asStringRef());
      llvm::Function* callee = mod->getFunction(funName);
      llvm::FunctionType* calleeType = callee->getFunctionType();
      return b->CreateCall(callee, args);
    }
    else if (auto e = ConstrExp::downcast(_exp)) {
      std::vector<llvm::Value*> args;
      for (Exp* arg : e->getFields()->asArrayRef())
        { args.push_back(genExp(arg)); }
      
      llvm::StructType* st = structTypes[e->getStruct()->asStringRef()];
      llvm::Value* mem = b->CreateAlloca(st);
      unsigned int fieldIdx = 0;
      for (auto argV : args) {
        llvm::Value* fieldAddr = b->CreateGEP(st, mem,
          { b->getInt64(0), b->getInt32(fieldIdx) });
        b->CreateStore(argV, fieldAddr);
        ++fieldIdx;
      }
      return b->CreateLoad(st, mem);
    }
    else if (auto e = DerefExp::downcast(_exp)) {
      llvm::Value* ofExp = genExp(e->getOf());
      llvm::Type* tyToLoad = genType(e->getType());
      return b->CreateLoad(tyToLoad, ofExp);
    }
    else if (auto e = NameExp::downcast(_exp)) {
      llvm::AllocaInst* variableAddress =
        varAddresses.getOrElse(e->getName()->asStringRef(), nullptr);
      assert(variableAddress && "Unbound variable");
      llvm::Type* varType = genType(e->getType());
      return b->CreateLoad(varType, variableAddress);
    }
    else if (auto e = IfExp::downcast(_exp)) {
      llvm::Function* f = b->GetInsertBlock()->getParent();
      llvm::Value* condition = genExp(e->getCondExp());
      auto thenBlock = llvm::BasicBlock::Create(llvmctx, "then");
      auto elseBlock = llvm::BasicBlock::Create(llvmctx, "else");
      auto contBlock = llvm::BasicBlock::Create(llvmctx, "ifcont");
      if (e->getElseExp() != nullptr)
        b->CreateCondBr(condition, thenBlock, elseBlock);
      else
        b->CreateCondBr(condition, thenBlock, contBlock);

      f->insert(f->end(), thenBlock);
      b->SetInsertPoint(thenBlock);
      llvm::Value* thenResult = genExp(e->getThenExp());
      b->CreateBr(contBlock);
      thenBlock = b->GetInsertBlock();

      llvm::Value* elseResult;
      if (e->getElseExp() != nullptr) {
        f->insert(f->end(), elseBlock);
        b->SetInsertPoint(elseBlock);
        elseResult = genExp(e->getElseExp());
        b->CreateBr(contBlock);
        elseBlock = b->GetInsertBlock();
      }

      f->insert(f->end(), contBlock);
      b->SetInsertPoint(contBlock);
      llvm::Type* retTy = genType(e->getType());
      if (!retTy->isVoidTy()) {
        llvm::PHINode* phiNode = b->CreatePHI(retTy, 2);
        phiNode->addIncoming(thenResult, thenBlock);
        phiNode->addIncoming(elseResult, elseBlock);
        return phiNode;
      } else {
        return nullptr;
      }
    }
    else if (auto e = IndexExp::downcast(_exp)) {
      llvm::Value* baseV = genExp(e->getBase());
      llvm::Value* indexV = genExp(e->getIndex());
      RefType* baseType = RefType::downcast(e->getBase()->getType());
      return b->CreateGEP(genType(baseType->inner), baseV, indexV);
    }
    else if (auto e = IntLit::downcast(_exp)) {
      return llvm::ConstantInt::get(genType(e->getType()), e->asLong());
    }
    else if (auto e = LetExp::downcast(_exp)) {
      llvm::Value* v = genExp(e->getDefinition());
      llvm::StringRef boundIdentName = e->getBoundIdent()->asStringRef();
      llvm::AllocaInst* memCell = b->CreateAlloca(v->getType());
      b->CreateStore(v, memCell);
      varAddresses.add(boundIdentName, memCell);
      memCell->setName(boundIdentName);
      return nullptr;
    }
    else if (auto e = MoveExp::downcast(_exp)) {
      return genExp(e->getRefExp());
    }
    else if (auto e = ProjectExp::downcast(_exp)) {
      return genProjectExp(e->getBase(), e->getFieldName(), e->getKind(),
        e->getTypeName());
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
    else if (auto e = WhileExp::downcast(_exp)) {
      llvm::Function* f = b->GetInsertBlock()->getParent();
      auto condBlock = llvm::BasicBlock::Create(llvmctx, "whileCondition");
      auto bodyBlock = llvm::BasicBlock::Create(llvmctx, "whileBody");
      auto contBlock = llvm::BasicBlock::Create(llvmctx, "whileCont");
      b->CreateBr(condBlock);

      f->insert(f->end(), condBlock);
      b->SetInsertPoint(condBlock);
      llvm::Value* condResult = genExp(e->getCond());
      b->CreateCondBr(condResult, bodyBlock, contBlock);

      f->insert(f->end(), bodyBlock);
      b->SetInsertPoint(bodyBlock);
      genExp(e->getBody());
      b->CreateBr(condBlock);

      f->insert(f->end(), contBlock);
      b->SetInsertPoint(contBlock);
      return nullptr;
    }
    llvm_unreachable("Codegen::genExp() fallthrough");
    return nullptr;
  }

  /// @brief Generates code for ProjectExp.
  llvm::Value* genProjectExp(Exp* base, Name* field, ProjectExp::Kind kind,
                             llvm::StringRef typeName) {
    llvm::Value* baseV = genExp(base);
    StructDecl* structDecl = ont.getType(typeName);
    unsigned int fieldIndex = 0;
    llvm::Type* fieldType;
    for (auto f : structDecl->getFields()->asArrayRef()) {
      if (f.first->asStringRef() == field->asStringRef()) {
        fieldType = genType(f.second);
        break;
      }
      ++fieldIndex;
    }
    assert(fieldIndex < structDecl->getFields()->asArrayRef().size());
    switch (kind) {
    case ProjectExp::DOT:
      return b->CreateExtractValue(baseV, fieldIndex);
    case ProjectExp::BRACKETS:
      return b->CreateGEP(structTypes[typeName], baseV,
        { b->getInt64(0), b->getInt32(fieldIndex) });
    case ProjectExp::ARROW:
      return b->CreateLoad(
        fieldType,
        b->CreateGEP(structTypes[typeName], baseV,
          { b->getInt64(0), b->getInt32(fieldIndex) })
      );
    }
    llvm_unreachable("Codegen::genProjectExp() switch case fallthrough");
    return nullptr;
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
      varAddresses.push();
      b->SetInsertPoint(llvm::BasicBlock::Create(llvmctx, "entry", f));
      initializeFunctionArguments(f, funDecl->getParameters());
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
      varAddresses.pop();
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
    else if (decl->id == AST::ID::STRUCT)
      {}
    else llvm_unreachable("Unsupported decl");
  }

  void genDeclList(DeclList* declList) {
    for (Decl* decl : declList->asArrayRef())
      genDecl(decl);
  }

};

#endif