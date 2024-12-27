#ifndef TYPER_CANONICALIZER
#define TYPER_CANONICALIZER

#include <llvm/ADT/Twine.h>
#include "common/AST.hpp"
#include "common/LocatedError.hpp"
#include "common/Ontology.hpp"

/// @brief Second of five type checking phases. Replaces all names in an AST
/// with their fully qualified names.
class Canonicalizer {
  const Ontology& ont;
  std::vector<LocatedError>& errors;

public:
  Canonicalizer(const Ontology& ont, std::vector<LocatedError>& errors)
    : ont(ont), errors(errors) {}
  Canonicalizer(const Canonicalizer&) = delete;

  /// @brief Recursively canonicalizes all the names in @p decl.
  /// @param scope the scope in which @p decl appears
  void run(Decl* decl, llvm::StringRef scope = "global") {
    llvm::Twine fqn = scope + "::" + decl->getName()->asStringRef();
    decl->getName()->set(fqn);
    if (auto structDecl = StructDecl::downcast(decl)) {
      canonicalizeNonDecl(scope, structDecl->getFields());
    }
    else if (auto funcDecl = FunctionDecl::downcast(decl)) {
      canonicalizeNonDecl(scope, funcDecl->getParameters());
      canonicalizeNonDecl(scope, funcDecl->getReturnType());
      Exp* body = funcDecl->getBody();
      if (body != nullptr) canonicalizeNonDecl(scope, body);
    }
    else if (auto moduleDecl = ModuleDecl::downcast(decl)) {
      run(moduleDecl->getDecls(), decl->getName()->asStringRef());
    }
  }

  /// @brief Recursively canonicalizes all names in @p declList
  /// @param scope the scope in which the decls in @p declList appear
  void run(DeclList* declList, llvm::StringRef scope = "global")
    { for (auto decl : declList->asArrayRef()) run(decl, scope); }

private:

  /// @brief Recursively canonicalizes @p ast which must not be or contain a
  /// declaration.
  /// @param scope the (deepest) module in which @p ast appears
  void canonicalizeNonDecl(llvm::StringRef scope, AST* ast) {
    if (auto nameTExp = NameTypeExp::downcast(ast)) {
      canonicalize(scope, nameTExp->getName(), Ontology::Space::TYPE);
    }
    else if (auto callExp = CallExp::downcast(ast)) {
      canonicalizeCallExpFunction(scope, callExp->getFunction());
      for (auto arg : callExp->getArguments()->asArrayRef())
        canonicalizeNonDecl(scope, arg);
    }
    else if (auto constrExp = ConstrExp::downcast(ast)) {
      canonicalizeConstrExpStruct(scope, constrExp->getStruct());
      for (auto arg : constrExp->getFields()->asArrayRef())
        canonicalizeNonDecl(scope, arg);
    }
    else {
      for (AST* node : ast->getASTChildren()) canonicalizeNonDecl(scope, node);
    }
  }

  /// @brief Canonicalizes the function name in a call expression.
  /// @param scope fully-qualified name of the (lowest) module in which the
  /// call expression appears.
  void canonicalizeCallExpFunction(llvm::StringRef scope, Name* functionName) {
    while (!scope.empty()) {
      std::string fqn = (scope + "::" + functionName->asStringRef()).str();
      if (auto d = ont.getFunction(fqn)) {
        functionName->set(fqn);
        return;
      }
      scope = getQualifier(scope);
    }
    errors.push_back(LocatedError()
      << "Function not found.\n" << functionName->getLocation()
    );
  }

  /// @brief Canonicalizes the struct name in a constructor invocation.
  /// @param scope fully-qualified name of the (lowest) module in which the
  /// ConstrExp appears.
  void canonicalizeConstrExpStruct(llvm::StringRef scope, Name* structName) {
    while (!scope.empty()) {
      std::string fqn = (scope + "::" + structName->asStringRef()).str();
      if (ont.getType(fqn) != nullptr) {
        structName->set(fqn);
        return;
      }
      scope = getQualifier(scope);
    }
    errors.push_back(LocatedError()
      << "Data type not found.\n" << structName->getLocation()
    );
  }

  /// @brief Fully-qualifies @p name, which appears in @p scope, by searching
  /// for a decl in @p space.
  void canonicalize(llvm::StringRef scope, Name* name, Ontology::Space space) {
    while (!scope.empty()) {
      std::string fqName = (scope + "::" + name->asStringRef()).str();
      Decl* d = ont.getDecl(fqName, space);
      if (d != nullptr) { name->set(fqName); return; }
      scope = getQualifier(scope);
    }
    errors.push_back(LocatedError()
      << "Failed to canonicalize name.\n" << name->getLocation()
    );
  }

  /// @brief Returns a reference to the qualifier of this name. Returns empty
  /// string if there is no qualifier.
  llvm::StringRef getQualifier(llvm::StringRef name) {
    auto idx = name.rfind(':');
    if (idx == std::string::npos) return "";
    return llvm::StringRef(name.data(), idx-1);
  }

};

#endif