#ifndef TYPER_TYPER
#define TYPER_TYPER

#include "typer/Cataloger.hpp"
#include "typer/Canonicalizer.hpp"
#include "typer/Unifier.hpp"
#include "typer/LValueMarker.hpp"
#include "typer/Resolver.hpp"

/// @brief The MiSCR type checker and type inference engine.
///
/// Type checking is divided into five sequential phases:
///   1. Cataloger     -- Builds a map of decl names to their AST definitions
///   2. Canonicalizer -- Fully qualifies all names in the AST
///   3. Unifier       -- Hindley-Milner type unification
///   4. LValueMarker  -- Distinguishes lvalues from rvalues
///   5. Resolver      -- Scrubs type variables from the AST
class Typer {
  Ontology ont;
  TypeContext tc;

  /// @brief Encodes type variable equivalence classes for union-find.
  /// Used by Unifier and Resolver.
  llvm::DenseMap<TypeVar*, TypeVar*> tvarEquiv;

  /// @brief Used by Unifier and Resolver.
  llvm::DenseMap<TypeVar*, Type*> tvarBindings;

  std::vector<LocatedError> errors;

public:
  Typer() {}
  Typer(const Typer&) = delete;

  const Ontology& getOntology() const { return ont; }
  TypeContext& getTypeContext() { return tc; }
  const std::vector<LocatedError>& getErrors() const { return errors; }

  /// @brief True iff at least one error has been produced by the type checker
  /// so far.
  bool hasErrors() const { return !errors.empty(); }

  /// @brief True iff no errors have been produced by the type checker so far.
  bool hasNoErrors() const { return errors.empty(); }

  /// @brief Runs the Unifier and Resolver on expression @p e.
  void typeExp(Exp* e) {
    Unifier(ont, tc, tvarEquiv, tvarBindings, errors).unifyExp(e);
    if (!errors.empty()) return;
    LValueMarker(errors).run(e);
    if (!errors.empty()) return;
    Resolver(tvarEquiv, tvarBindings, tc).resolveAST(e);
  }

  /// @brief Runs the full type checking procedure on declaration @p d.
  void typeDecl(Decl* d) {
    Cataloger(ont, errors).run(d);
    if (!errors.empty()) return;
    Canonicalizer(ont, errors).run(d);
    if (!errors.empty()) return;
    Unifier(ont, tc, tvarEquiv, tvarBindings, errors).unifyDecl(d);
    if (!errors.empty()) return;
    LValueMarker(errors).run(d);
    if (!errors.empty()) return;
    Resolver(tvarEquiv, tvarBindings, tc).resolveAST(d);
  }

  /// @brief Runs the full type checking procedure on @p decls.
  void typeDeclList(DeclList* decls) {
    Cataloger(ont, errors).run(decls);
    if (!errors.empty()) return;
    Canonicalizer(ont, errors).run(decls);
    if (!errors.empty()) return;
    Unifier(ont, tc, tvarEquiv, tvarBindings, errors).unifyDeclList(decls);
    if (!errors.empty()) return;
    LValueMarker(errors).run(decls);
    if (!errors.empty()) return;
    Resolver(tvarEquiv, tvarBindings, tc).resolveAST(decls);
  }

};

#endif