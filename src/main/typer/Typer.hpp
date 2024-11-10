#ifndef TYPER_TYPER
#define TYPER_TYPER

#include "typer/Cataloger.hpp"
#include "typer/Canonicalizer.hpp"
#include "typer/Unifier.hpp"

/// @brief The MiSCR type checker and type inference engine.
///
/// Type checking is divided into three sequential phases:
///   1. Cataloger     -- Builds a map of decl names to their AST definitions
///   2. Canonicalizer -- Fully qualifies all names in the AST
///   3. Unifier       -- Hindley-Milner type unification
class Typer {
  Ontology ont;
  TypeContext tc;
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

  void typeExp(Exp* e) {
    Unifier(ont, tc, errors).unifyExp(e);
  }

  void typeDecl(Decl* d) {
    Cataloger(ont, errors).run(d);
    if (!errors.empty()) return;
    Canonicalizer(ont, errors).run(d);
    if (!errors.empty()) return;
    Unifier(ont, tc, errors).unifyDecl(d);
  }

  void typeDeclList(DeclList* decls) {
    Cataloger(ont, errors).run(decls);
    if (!errors.empty()) return;
    Canonicalizer(ont, errors).run(decls);
    if (!errors.empty()) return;
    Unifier(ont, tc, errors).unifyDeclList(decls);
  }

};

#endif