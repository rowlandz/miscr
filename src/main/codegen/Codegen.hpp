#ifndef CODEGEN_CODEGEN
#define CODEGEN_CODEGEN

#include <llvm/IR/IRBuilder.h>
#include "common/ScopeStack.hpp"
#include "common/TypeContext.hpp"
#include "common/Ontology.hpp"

/// @brief LLVM IR code generation from AST.
class Codegen {
  const Ontology& ont;
  llvm::Module& mod;
  llvm::IRBuilder<> B;

  /// @brief Maps fully qualified names of structs to their LLVM struct types.
  llvm::StringMap<llvm::StructType*> structTypes;

  /// @brief Maps local variable names to their (stack) addresses.
  ScopeStack<llvm::AllocaInst*> varAddresses;

public:

  /// @brief Creates and initializes a Codegen object. Stubs of all functions
  /// in @p ont are added to @p mod.
  Codegen(const Ontology& ont, llvm::Module& mod)
    : ont(ont), mod(mod), B(mod.getContext()) {

    // Populates `structTypes`
    for (llvm::StringRef typeName : ont.typeSpace.keys()) {
      std::vector<llvm::Type*> fieldTys;
      StructDecl* structDecl = ont.getType(typeName);
      for (auto field : structDecl->getFields()->asArrayRef())
        fieldTys.push_back(genType(field.second));
      auto st = llvm::StructType::get(B.getContext(), fieldTys);
      st->setName(typeName);
      structTypes[typeName] = st;
    }

    // Add all functions to the LLVM module
    for (llvm::StringRef funName : ont.functionSpace.keys()) {
      FunctionDecl* funDecl = ont.getFunction(funName);
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
    }
  }

  ~Codegen() {}
  Codegen(const Codegen&) = delete;
  Codegen& operator=(const Codegen&) = delete;

  /// @brief Recursively generates code for all decls in @p declList.
  void genDeclList(DeclList* declList)
    { for (Decl* decl : declList->asArrayRef()) genDecl(decl); }

  /// @brief Recursively generates code for @p decl.
  void genDecl(Decl* decl) {
    if (auto mod = ModuleDecl::downcast(decl))
      genModule(mod);
    else if (auto func = FunctionDecl::downcast(decl))
      genFuncBody(func);
    else if (decl->id == AST::ID::STRUCT)
      {}
    else llvm_unreachable("Unsupported decl");
  }

  /// @brief Recursively generates code for all decls in @p mod
  void genModule(ModuleDecl* mod) { genDeclList(mod->getDecls()); }

private:

  /// @brief Generates the body of @p funDecl, or does nothing if the function
  /// is extern. The llvm::Function* (with no body) must already be in `mod`.
  void genFuncBody(FunctionDecl* funDecl) {
    llvm::Function* f = mod.getFunction(
      ont.mapName(funDecl->getName()->asStringRef()));
    assert(f != nullptr && "Function not found in LLVM module");
    if (!funDecl->hasBody()) return;
    varAddresses.push();
    B.SetInsertPoint(llvm::BasicBlock::Create(B.getContext(), "entry", f));
    initializeFunctionArguments(f, funDecl->getParameters());
    llvm::Value* retVal = genExp(funDecl->getBody());
    // TODO: this is gross
    if (auto primRT = PrimitiveTypeExp::downcast(funDecl->getReturnType())) {
      if (primRT->kind == PrimitiveTypeExp::UNIT)
        B.CreateRetVoid();
      else
        B.CreateRet(retVal);
    } else {
      B.CreateRet(retVal);
    }
    varAddresses.pop();
  }

  /// @brief Initializes stack memory for function arguments. The insertion
  /// point of `B` must be the beginning of @p f.
  void initializeFunctionArguments(llvm::Function* f, ParamList* paramList) {
    auto params = paramList->asArrayRef();
    unsigned int i = 0;
    for (llvm::Argument &arg : f->args()) {
      llvm::StringRef paramName = params[i].first->asStringRef();
      llvm::AllocaInst* memCell = B.CreateAlloca(arg.getType());
      B.CreateStore(&arg, memCell);
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
  llvm::Value* genExp(Exp* exp) {
    if (auto e = BinopExp::downcast(exp)) {
      llvm::Value* v1 = genExp(e->getLHS());
      llvm::Value* v2 = genExp(e->getRHS());
      switch (e->getBinop()) {
      case BinopExp::ADD: return B.CreateAdd(v1, v2);
      case BinopExp::AND: return B.CreateAnd(v1, v2);
      case BinopExp::DIV: return B.CreateSDiv(v1, v2);
      case BinopExp::GE:  return B.CreateICmpSGE(v1, v2);
      case BinopExp::GT:  return B.CreateICmpSGT(v1, v2);
      case BinopExp::LE:  return B.CreateICmpSLE(v1, v2);
      case BinopExp::LT:  return B.CreateICmpSLT(v1, v2);
      case BinopExp::MOD: return B.CreateSRem(v1, v2);
      case BinopExp::MUL: return B.CreateMul(v1, v2);
      case BinopExp::OR:  return B.CreateOr(v1, v2);
      case BinopExp::SUB: return B.CreateSub(v1, v2);
      case BinopExp::EQ: {
        llvm::Type* operandType = v1->getType();
        if (operandType->isIntegerTy()) return B.CreateICmpEQ(v1, v2);
        if (operandType->isFloatingPointTy()) return B.CreateFCmpOEQ(v1, v2);
        llvm_unreachable("Unsupported type for == operator");
      }
      case BinopExp::NE: {
        llvm::Type* operandType = v1->getType();
        if (operandType->isIntegerTy()) return B.CreateICmpNE(v1, v2);
        if (operandType->isFloatingPointTy()) return B.CreateFCmpONE(v1, v2);
        llvm_unreachable("Unsupported type for /= operator");
      }
      default: llvm_unreachable("Unsupported binary operator");
      }
    }
    else if (auto e = AddrOfExp::downcast(exp)) {
      return genExpByReference(e->getOf());
    }
    else if (auto e = AscripExp::downcast(exp)) {
      return genExp(e->getAscriptee());
    }
    else if (auto e = AssignExp::downcast(exp)) {
      llvm::Value* lhsAddr = genExpByReference(e->getLHS());
      llvm::Value* rhs = genExp(e->getRHS());
      B.CreateStore(rhs, lhsAddr);
      return nullptr;
    }
    else if (auto e = BlockExp::downcast(exp)) {
      llvm::ArrayRef<Exp*> stmtList = e->getStatements()->asArrayRef();
      llvm::Value* lastStmtVal = nullptr;
      for (Exp* stmt : stmtList) {
        lastStmtVal = genExp(stmt);
      }
      return lastStmtVal;
    }
    else if (auto e = BoolLit::downcast(exp)) {
      return B.getInt1(e->getValue());
    }
    else if (auto e = BorrowExp::downcast(exp)) {
      return genExp(e->getRefExp());
    }
    else if (auto e = CallExp::downcast(exp)) {
      std::vector<llvm::Value*> args;
      for (Exp* arg : e->getArguments()->asArrayRef())
        { args.push_back(genExp(arg)); }

      llvm::StringRef funName = ont.mapName(e->getFunction()->asStringRef());
      llvm::Function* callee = mod.getFunction(funName);
      llvm::FunctionType* calleeType = callee->getFunctionType();
      return B.CreateCall(callee, args);
    }
    else if (auto e = ConstrExp::downcast(exp)) {
      std::vector<llvm::Value*> args;
      for (Exp* arg : e->getFields()->asArrayRef())
        { args.push_back(genExp(arg)); }
      
      llvm::StructType* st = structTypes[e->getStruct()->asStringRef()];
      llvm::Value* mem = B.CreateAlloca(st);
      unsigned int fieldIdx = 0;
      for (auto argV : args) {
        llvm::Value* fieldAddr = B.CreateGEP(st, mem,
          { B.getInt64(0), B.getInt32(fieldIdx) });
        B.CreateStore(argV, fieldAddr);
        ++fieldIdx;
      }
      return B.CreateLoad(st, mem);
    }
    else if (auto e = DerefExp::downcast(exp)) {
      llvm::Value* ofExp = genExp(e->getOf());
      llvm::Type* tyToLoad = genType(e->getType());
      return B.CreateLoad(tyToLoad, ofExp);
    }
    else if (auto e = NameExp::downcast(exp)) {
      llvm::AllocaInst* variableAddress =
        varAddresses.getOrElse(e->getName()->asStringRef(), nullptr);
      assert(variableAddress && "Unbound variable");
      llvm::Type* varType = genType(e->getType());
      return B.CreateLoad(varType, variableAddress);
    }
    else if (auto e = IfExp::downcast(exp)) {
      llvm::Function* f = B.GetInsertBlock()->getParent();
      llvm::Value* condition = genExp(e->getCondExp());
      auto thenBlock = llvm::BasicBlock::Create(B.getContext(), "then");
      auto elseBlock = llvm::BasicBlock::Create(B.getContext(), "else");
      auto contBlock = llvm::BasicBlock::Create(B.getContext(), "ifcont");
      if (e->getElseExp() != nullptr)
        B.CreateCondBr(condition, thenBlock, elseBlock);
      else
        B.CreateCondBr(condition, thenBlock, contBlock);

      f->insert(f->end(), thenBlock);
      B.SetInsertPoint(thenBlock);
      llvm::Value* thenResult = genExp(e->getThenExp());
      B.CreateBr(contBlock);
      thenBlock = B.GetInsertBlock();

      llvm::Value* elseResult;
      if (e->getElseExp() != nullptr) {
        f->insert(f->end(), elseBlock);
        B.SetInsertPoint(elseBlock);
        elseResult = genExp(e->getElseExp());
        B.CreateBr(contBlock);
        elseBlock = B.GetInsertBlock();
      }

      f->insert(f->end(), contBlock);
      B.SetInsertPoint(contBlock);
      llvm::Type* retTy = genType(e->getType());
      if (!retTy->isVoidTy()) {
        llvm::PHINode* phiNode = B.CreatePHI(retTy, 2);
        phiNode->addIncoming(thenResult, thenBlock);
        phiNode->addIncoming(elseResult, elseBlock);
        return phiNode;
      } else {
        return nullptr;
      }
    }
    else if (auto e = IndexExp::downcast(exp)) {
      llvm::Value* baseV = genExp(e->getBase());
      llvm::Value* indexV = genExp(e->getIndex());
      RefType* baseType = RefType::downcast(e->getBase()->getType());
      return B.CreateGEP(genType(baseType->inner), baseV, indexV);
    }
    else if (auto e = IntLit::downcast(exp)) {
      return llvm::ConstantInt::get(genType(e->getType()), e->asLong());
    }
    else if (auto e = LetExp::downcast(exp)) {
      llvm::Value* v = genExp(e->getDefinition());
      llvm::StringRef boundIdentName = e->getBoundIdent()->asStringRef();
      llvm::AllocaInst* memCell = B.CreateAlloca(v->getType());
      B.CreateStore(v, memCell);
      varAddresses.add(boundIdentName, memCell);
      memCell->setName(boundIdentName);
      return nullptr;
    }
    else if (auto e = MoveExp::downcast(exp)) {
      return genExp(e->getRefExp());
    }
    else if (auto e = ProjectExp::downcast(exp)) {
      return genProjectExp(e->getBase(), e->getFieldName(), e->getKind(),
        e->getTypeName());
    }
    else if (auto e = StringLit::downcast(exp)) {
      return B.CreateGlobalString(e->processEscapes());
    }
    else if (auto e = UnopExp::downcast(exp)) {
      llvm::Value* innerV = genExp(e->getInner());
      switch (e->getUnop()) {
      case UnopExp::NEG: return B.CreateNeg(innerV);
      case UnopExp::NOT: return B.CreateNot(innerV);
      }
    }
    else if (auto e = WhileExp::downcast(exp)) {
      llvm::Function* f = B.GetInsertBlock()->getParent();
      auto condBlock = llvm::BasicBlock::Create(B.getContext(), "whileCond");
      auto bodyBlock = llvm::BasicBlock::Create(B.getContext(), "whileBody");
      auto contBlock = llvm::BasicBlock::Create(B.getContext(), "whileCont");
      B.CreateBr(condBlock);

      f->insert(f->end(), condBlock);
      B.SetInsertPoint(condBlock);
      llvm::Value* condResult = genExp(e->getCond());
      B.CreateCondBr(condResult, bodyBlock, contBlock);

      f->insert(f->end(), bodyBlock);
      B.SetInsertPoint(bodyBlock);
      genExp(e->getBody());
      B.CreateBr(condBlock);

      f->insert(f->end(), contBlock);
      B.SetInsertPoint(contBlock);
      return nullptr;
    }
    llvm_unreachable("Codegen::genExp() fallthrough");
    return nullptr;
  }

  /// @brief Generates code for a ProjectExp.
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
      return B.CreateExtractValue(baseV, fieldIndex);
    case ProjectExp::BRACKETS:
      return B.CreateGEP(structTypes[typeName], baseV,
        { B.getInt64(0), B.getInt32(fieldIndex) });
    case ProjectExp::ARROW:
      return B.CreateLoad(
        fieldType,
        B.CreateGEP(structTypes[typeName], baseV,
          { B.getInt64(0), B.getInt32(fieldIndex) })
      );
    }
    llvm_unreachable("Codegen::genProjectExp() switch case fallthrough");
    return nullptr;
  }

  /// @brief Converts a Type to an llvm::Type
  llvm::Type* genType(Type* ty) {
    if (auto constraint = Constraint::downcast(ty)) {
      switch (constraint->kind) {
      case Constraint::DECIMAL:   return B.getDoubleTy();
      case Constraint::NUMERIC:   return B.getInt32Ty();
      }
    }
    if (auto nameTy = NameType::downcast(ty)) {
      return structTypes[nameTy->asString];
    }
    if (auto primTy = PrimitiveType::downcast(ty)) {
      switch (primTy->kind) {
      case PrimitiveType::BOOL:   return B.getInt1Ty();
      case PrimitiveType::f32:    return B.getFloatTy();
      case PrimitiveType::f64:    return B.getDoubleTy();
      case PrimitiveType::i8:     return B.getInt8Ty();
      case PrimitiveType::i16:    return B.getInt16Ty();
      case PrimitiveType::i32:    return B.getInt32Ty();
      case PrimitiveType::i64:    return B.getInt64Ty();
      case PrimitiveType::UNIT:   return B.getVoidTy();
      }
    }
    if (auto refTy = RefType::downcast(ty)) {
      return llvm::PointerType::get(B.getContext(), 0);
    }
    if (auto tyVar = TypeVar::downcast(ty)) {
      llvm_unreachable("TypeVars should not appear after Resolver");
    }
    llvm_unreachable("Codegen::genType(TVar) case unimplemented");
  }

  /// @brief Converts a type expression to an llvm::Type.
  llvm::Type* genType(TypeExp* texp) {
    if (auto primTexp = PrimitiveTypeExp::downcast(texp)) {
      switch (primTexp->kind) {
      case PrimitiveTypeExp::BOOL:   return B.getInt1Ty();
      case PrimitiveTypeExp::f32:    return B.getFloatTy();
      case PrimitiveTypeExp::f64:    return B.getDoubleTy();
      case PrimitiveTypeExp::i8:     return B.getInt8Ty();
      case PrimitiveTypeExp::i16:    return B.getInt16Ty();
      case PrimitiveTypeExp::i32:    return B.getInt32Ty();
      case PrimitiveTypeExp::i64:    return B.getInt64Ty();
      case PrimitiveTypeExp::UNIT:   return B.getVoidTy();
      }
    }
    if (auto nameTexp = NameTypeExp::downcast(texp)) {
      return structTypes[nameTexp->getName()->asStringRef()];
    }
    if (RefTypeExp::downcast(texp)) {
      return llvm::PointerType::get(B.getContext(), 0);
    }
    llvm_unreachable("Codegen::genType(TypeExp*) case unimplemented");
  }

};

#endif