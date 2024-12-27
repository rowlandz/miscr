#ifndef TYPER_CATALOGER
#define TYPER_CATALOGER

#include "common/AST.hpp"
#include "common/LocatedError.hpp"
#include "common/Ontology.hpp"

/// @brief First of five type checking phases. Populates an Ontology with the
/// names of decls and pointers to their definitions in the AST.
class Cataloger {
  Ontology& ont;
  std::vector<LocatedError>& errors;

public:
  Cataloger(Ontology& ont, std::vector<LocatedError>& errors)
    : ont(ont), errors(errors) {}
  Cataloger(const Cataloger&) = delete;

  /// @brief Recursively catalogs a decl that appears in @p scope.
  void run(Decl* decl, llvm::StringRef scope = "global") {
    if (auto mod = ModuleDecl::downcast(decl)) {
      llvm::StringRef relName = mod->getName()->asStringRef();
      std::string fqn = (scope + "::" + relName).str();
      if (auto existingModule = ont.getModule(fqn)) {
        errors.push_back(LocatedError()
          << "Duplicate module definition.\n" << decl->getName()->getLocation()
          << "Previous definition was here:\n"
          << existingModule->getName()->getLocation()
        );
      } else {
        ont.record(fqn, mod);
      }
      run(mod->getDecls(), fqn);
    }
    else if (auto structDecl = StructDecl::downcast(decl)) {
      llvm::StringRef relName = structDecl->getName()->asStringRef();
      std::string fqn = (scope + "::" + relName).str();
      if (auto prevDef = ont.getDecl(fqn, Ontology::Space::TYPE)) {
        errors.push_back(LocatedError()
          << "Data type is already defined.\n"
          << decl->getName()->getLocation()
          << "Previous definition was here:\n"
          << prevDef->getName()->getLocation()
        );
      } else {
        ont.record(fqn, structDecl);
      }
    }
    else if (auto func = FunctionDecl::downcast(decl)) {
      llvm::StringRef relName = func->getName()->asStringRef();
      std::string fqn = (scope + "::" + relName).str();
      if (auto prevDef = ont.getFunction(fqn)) {
        errors.push_back(LocatedError()
          << "Function is already defined.\n" << decl->getName()->getLocation()
          << "Previous definition was here:\n"
          << prevDef->getName()->getLocation()
        );
      }
      if (relName.equals("main")) {
        if (!ont.entryPoint.empty()) {
          errors.push_back(LocatedError()
            << "There are multiple program entry points.\n"
            << decl->getName()->getLocation()
            // TODO: add "previous entry point is here ..."
          );
          return;
        }
        ont.recordMapName(fqn, func, "main");
      } else if (func->isExtern()) {
        std::string relNameString = relName.str();
        ont.recordMapName(fqn, func, relNameString);
      } else {
        ont.record(fqn, func);
      }
    }
  }

  /// @brief Catalogs the decls in a decl list. `scope` is the scope that the
  /// decls appear in.
  void run(DeclList* declList, llvm::StringRef scope = "global")
    { for (auto decl : declList->asArrayRef()) run(decl, scope); }

};

#endif