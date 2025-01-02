#ifndef SEMA_SEMA
#define SEMA_SEMA

#include "sema/Cataloger.hpp"
#include "sema/Canonicalizer.hpp"
#include "sema/Unifier.hpp"
#include "sema/LValueMarker.hpp"
#include "sema/Resolver.hpp"

/// @brief The semantic analyzer.
///
/// Semantic analysis is consists of five sub-tasks:
///   1. Cataloger     -- Builds a map of decl names to their AST definitions
///   2. Canonicalizer -- Fully qualifies all names in the AST
///   3. Unifier       -- Hindley-Milner type unification
///   4. LValueMarker  -- Distinguishes lvalues from rvalues
///   5. Resolver      -- Scrubs type variables from the AST
///
/// Cataloguing is run over the entire parsed AST. Then the next four sub-tasks
/// are run once per non-module decl and can run in parallel.
class Sema {
  Ontology ont;
  TypeContext tc;

  /// @brief Encodes type variable equivalence classes for union-find.
  /// Used by Unifier and Resolver.
  llvm::DenseMap<TypeVar*, TypeVar*> tvarEquiv;

  /// @brief Used by Unifier and Resolver.
  llvm::DenseMap<TypeVar*, Type*> tvarBindings;

  std::vector<LocatedError> errors;

public:
  Sema() {}
  Sema(const Sema&) = delete;

  const Ontology& getOntology() const { return ont; }
  TypeContext& getTypeContext() { return tc; }
  const std::vector<LocatedError>& getErrors() const { return errors; }

  /// @brief True iff at least one error has been produced so far.
  bool hasErrors() const { return !errors.empty(); }

  /// @brief True iff no errors have been produced so far.
  bool hasNoErrors() const { return errors.empty(); }

  /// @brief Runs all semantic analysis tasks on @p decls. 
  void run(DeclList* decls, llvm::StringRef scope) {
    Cataloger(ont, errors).run(decls, scope);
    if (!errors.empty()) return;
    analyzeDeclList(decls, scope);
  }

  /// @brief Runs all semantic analysis tasks on @p decl in the global scope.
  void run(Decl* decl, llvm::StringRef scope) {
    Cataloger(ont, errors).run(decl, scope);
    if (!errors.empty()) return;
    analyzeDecl(decl, scope);
  }

  /// @brief Runs all sema tasks except cataloging over @p e.
  void analyzeExp(Exp* e, llvm::StringRef scope) {
    Canonicalizer(ont, errors).run(e, scope);
    if (!errors.empty()) return;
    Unifier(ont, tc, tvarEquiv, tvarBindings, errors).unifyExp(e);
    if (!errors.empty()) return;
    LValueMarker(errors).run(e);
    if (!errors.empty()) return;
    Resolver(tvarEquiv, tvarBindings, tc).resolveAST(e);
  }

private:

  /// @brief Runs all sema tasks except cataloging over @p decls
  void analyzeDeclList(DeclList* decls, llvm::StringRef scope)
    { for (Decl* decl : decls->asArrayRef()) analyzeDecl(decl, scope); }

  /// @brief Runs all sema tasks except cataloging over @p decl.
  void analyzeDecl(Decl* decl, llvm::StringRef scope) {
    if (auto funcDecl = FunctionDecl::downcast(decl))
      analyzeFuncDecl(funcDecl, scope);
    else if (auto modDecl = ModuleDecl::downcast(decl))
      analyzeDeclList(modDecl->getDecls(),
        (scope + "::" + modDecl->getName()->asStringRef()).str());
    else if (auto structDecl = StructDecl::downcast(decl))
      analyzeStructDecl(structDecl, scope);
  }

  /// @brief Runs all sema tasks except cataloging over @p f.
  void analyzeFuncDecl(FunctionDecl* f, llvm::StringRef scope) {
    Canonicalizer(ont, errors).run(f, scope);
    if (!errors.empty()) return;
    Unifier(ont, tc, tvarEquiv, tvarBindings, errors).unifyFunc(f);
    if (!errors.empty()) return;
    LValueMarker(errors).run(f);
    if (!errors.empty()) return;
    Resolver(tvarEquiv, tvarBindings, tc).resolveAST(f);
  }

  /// @brief Runs all sema tasks except cataloging over @p s.
  void analyzeStructDecl(StructDecl* s, llvm::StringRef scope)
    { Canonicalizer(ont, errors).run(s, scope); }

};

#endif