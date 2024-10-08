#ifndef TYPER_CATALOGER
#define TYPER_CATALOGER

#include "llvm/ADT/Twine.h"
#include "common/AST.hpp"
#include "common/LocatedError.hpp"
#include "common/Ontology.hpp"

/// @brief First of the two type checking phases. Fully qualifies all decl names
/// and builds an `Ontology`.
class Cataloger {
  Ontology& ont;
  std::vector<LocatedError>& errors;

public:
  Cataloger(Ontology& ont, std::vector<LocatedError>& errors)
    : ont(ont), errors(errors) {}
  
  void run(Decl* decl) {
    catalog("global", decl);
    canonicalize("global", decl);
  }

  void run(DeclList* decls) {
    catalogDeclList("global", decls);
    canonicalize("global", decls);
  }

private:

  /// Recursively catalogs a decl that appears in `scope`.
  void catalog(llvm::StringRef scope, Decl* _decl) {
    if (auto mod = ModuleDecl::downcast(_decl)) {
      llvm::StringRef relName = mod->getName()->asStringRef();
      std::string fqn = (scope + "::" + relName).str();
      if (auto existingModule = ont.getModule(fqn)) {
        LocatedError err;
        err.append("Duplicate module definition.\n");
        err.append(_decl->getName()->getLocation());
        err.append("Previous definition was here:\n");
        err.append(existingModule->getName()->getLocation());
        errors.push_back(err);
      } else {
        ont.record(fqn, mod);
      }
      catalogDeclList(fqn, mod->getDecls());
    }
    else if (auto data = DataDecl::downcast(_decl)) {
      llvm::StringRef relName = data->getName()->asStringRef();
      std::string fqn = (scope + "::" + relName).str();
      if (auto prevDef = ont.getDecl(fqn, Ontology::Space::FUNCTION_OR_TYPE)) {
        LocatedError err;
        err.append("Data type is already defined.\n");
        err.append(_decl->getName()->getLocation());
        err.append("Previous definition was here:\n");
        err.append(prevDef->getName()->getLocation());
        errors.push_back(err);
      } else {
        ont.record(fqn, data);
      }
    }
    else if (auto func = FunctionDecl::downcast(_decl)) {
      llvm::StringRef relName = func->getName()->asStringRef();
      std::string fqn = (scope + "::" + relName).str();
      if (auto prevDef = ont.getFunctionOrConstructor(fqn)) {
        LocatedError err;
        err.append("Function is already defined.\n");
        err.append(_decl->getName()->getLocation());
        err.append("Previous definition was here:\n");
        err.append(prevDef->getName()->getLocation());
        errors.push_back(err);
      }
      if (relName.equals("main")) {
        if (!ont.entryPoint.empty()) {
          LocatedError err;
          err.append("There are multiple program entry points.\n");
          err.append(_decl->getName()->getLocation());
          // TODO: add "previous entry point is here ..."
          return;
        }
        ont.recordMapName(fqn, func, "main");
      } else if (func->getID() == AST::ID::EXTERN_FUNC) {
        std::string relNameString = relName.str();
        ont.recordMapName(fqn, func, relNameString);
      } else {
        ont.record(fqn, func);
      }
    }
  }

  /// Catalogs the decls in a decl list. `scope` is the scope that the decls
  /// appear in.
  void catalogDeclList(llvm::StringRef scope, DeclList* declList) {
    for (auto decl : declList->asArrayRef()) {
      catalog(scope, decl);
    }
  }


  void canonicalize(llvm::StringRef scope, DeclList* declList) {
    for (auto decl : declList->asArrayRef()) canonicalize(scope, decl);
  }

  void canonicalize(llvm::StringRef scope, Decl* decl) {
    llvm::Twine fqn = scope + "::" + decl->getName()->asStringRef();
    decl->getName()->set(fqn);
    if (auto moduleDecl = ModuleDecl::downcast(decl)) {
      canonicalize(decl->getName()->asStringRef(), moduleDecl->getDecls());
    }
    else if (auto funcDecl = FunctionDecl::downcast(decl)) {
      for (auto param : funcDecl->getParameters()->asArrayRef())
        canonicalizeNonDecl(scope, param.second);
      canonicalizeNonDecl(scope, funcDecl->getReturnType());
      Exp* body = funcDecl->getBody();
      if (body != nullptr) canonicalizeNonDecl(scope, body);
    }
  }

  /// @brief Recursively canonicalizes `ast` which must not be or contain a
  /// declaration.
  /// @param scope The (deepest) module or namespace in which `ast` appears.
  void canonicalizeNonDecl(llvm::StringRef scope, AST* ast) {
    if (auto nameTExp = NameTypeExp::downcast(ast)) {
      canonicalize(scope, nameTExp->getName(), Ontology::Space::TYPE);
    }
    else if (auto callExp = CallExp::downcast(ast)) {
      canonicalizeCallExp(scope, callExp);
      for (auto arg : callExp->getArguments()->asArrayRef())
        canonicalizeNonDecl(scope, arg);
    }
    else {
      for (AST* node : getSubASTs(ast)) canonicalizeNonDecl(scope, node);
    }
  }

  /// @brief Canonicalizes the function name in a call expression. Flips CALL
  /// to CONSTR if found to be a constructor invocation.
  void canonicalizeCallExp(llvm::StringRef scope, CallExp* callExp) {
    Name* functionName = callExp->getFunction();
    while (!scope.empty()) {
      std::string fqn = (scope + "::" + functionName->asStringRef()).str();
      if (auto d = ont.getFunction(fqn)) {
        functionName->set(fqn);
        return;
      }
      if (auto d = ont.getType(fqn)) {
        functionName->set(fqn);
        callExp->markAsConstr();
        return;
      }
      scope = getQualifier(scope);
    }
    LocatedError err;
    err.append("Function or data type not found.\n");
    err.append(functionName->getLocation());
    errors.push_back(err);
  }

  /// @brief Fully-qualifies `name`, which appears in `scope`, by searching for
  /// a decl in `space`.
  void canonicalize(llvm::StringRef scope, Name* name, Ontology::Space space) {
    while (!scope.empty()) {
      std::string fqName = (scope + "::" + name->asStringRef()).str();
      Decl* d = ont.getDecl(fqName, space);
      if (d != nullptr) { name->set(fqName); return; }
      scope = getQualifier(scope);
    }
    LocatedError err;
    err.append("Failed to canonicalize name.\n");
    err.append(name->getLocation());
    errors.push_back(err);
  }

  /// Returns a reference to the qualifier of this name. Returns empty string
  /// if there is no qualifier.
  llvm::StringRef getQualifier(llvm::StringRef name) {
    auto idx = name.rfind(':');
    if (idx == std::string::npos) return "";
    return llvm::StringRef(name.data(), idx-1);
  }
};

#endif