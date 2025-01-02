#ifndef SEMA_LVALUEMARKER
#define SEMA_LVALUEMARKER

#include "common/AST.hpp"
#include "common/LocatedError.hpp"

/// @brief Fourth of five sema phases. Distinguishes between lvalue and rvalue
/// expressions.
///
/// An lvalue is an expression that for sure has an address. It is precisely
/// one of the following expression forms:
///   - AscripExp (iff the ascriptee is an lvalue)
///   - DerefExp
///   - NameExp
///   - ProjectExp with ARROW kind
///   - ProjectExp with DOT kind (iff `base` is an lvalue)
///
/// All other expressions are rvalues. The lvalue marker will emit an error if
/// it finds an rvalue as the LHS of an assignment or as the inner expression
/// of an AddrOfExp.
class LValueMarker {
  std::vector<LocatedError>& errors;

public:
  LValueMarker(std::vector<LocatedError>& errors) : errors(errors) {}

  /// @brief Recursively runs the Lvalue marker over @p funcDecl.
  void run(FunctionDecl* funcDecl)
    { if (funcDecl->getBody()) runNonDecl(funcDecl->getBody()); }

  /// @brief Recursively runs the lvalue marker over @p exp.
  void run(Exp* exp) { runNonDecl(exp); }

private:

  void runNonDecl(AST* ast) {
    if (auto e = AscripExp::downcast(ast)) {
      runNonDecl(e->getAscriptee());
      if (e->getAscriptee()->isLvalue()) e->markLvalue();
    }
    else if (auto e = AddrOfExp::downcast(ast)) {
      runNonDecl(e->getOf());
      if (!e->getOf()->isLvalue())
        errors.push_back(LocatedError()
          << "Expression must be an lvalue to get address:\n"
          << e->getOf()->getLocation()
        );
    }
    else if (auto e = AssignExp::downcast(ast)) {
      runNonDecl(e->getLHS());
      if (!e->getLHS()->isLvalue())
        errors.push_back(LocatedError()
          << "Left side of assignment is not an lvalue:\n"
          << e->getLHS()->getLocation()
        );
      runNonDecl(e->getRHS());
    }
    else if (auto e = DerefExp::downcast(ast)) {
      runNonDecl(e->getOf());
      e->markLvalue();
    }
    else if (auto e = NameExp::downcast(ast)) {
      e->markLvalue();
    }
    else if (auto e = ProjectExp::downcast(ast)) {
      runNonDecl(e->getBase());
      if (e->getKind() == ProjectExp::Kind::ARROW)
        e->markLvalue();
      else if (e->getKind() == ProjectExp::Kind::DOT)
        { if (e->getBase()->isLvalue()) e->markLvalue(); }
    }
    else {
      for (auto child : ast->getASTChildren()) runNonDecl(child);
    }
  }
};

#endif