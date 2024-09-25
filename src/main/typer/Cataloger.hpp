#ifndef TYPER_CATALOGER
#define TYPER_CATALOGER

#include "llvm/ADT/Twine.h"
#include "common/AST.hpp"
#include "common/Ontology.hpp"

/// @brief First of the two type checking phases. Fully qualifies all decl names
/// and builds an `Ontology`.
class Cataloger {
  Ontology* ont;
  std::vector<std::string> errors;

public:
  Cataloger(Ontology* ont) {
    this->ont = ont;
  }

  /// Catalogs a decl. `scope` is the scope that `_decl` appears in.
  void catalog(llvm::StringRef scope, Decl* _decl) {
    
    if (auto mod = ModuleDecl::downcast(_decl)) {
      llvm::StringRef relName = mod->getName()->asStringRef();
      std::string fqNameStr = (scope + "::" + relName).str();

      if (auto existingModule = ont->getModule(fqNameStr)) {
        moduleNameCollisionError(fqNameStr);
      } else {
        mod->setName(fqNameStr);
        ont->record(mod);
      }

      catalogDeclList(fqNameStr, mod->getDecls());
    }

    else if (auto data = DataDecl::downcast(_decl)) {
      llvm::StringRef relName = data->getName()->asStringRef();
      std::string fqName = (scope + "::" + relName).str();

      if (auto collidingDecl = ont->getFunctionOrConstructor(fqName)) {
        typeNameCollisionError(fqName);
      } else {
        data->setName(fqName);
        ont->record(data);
      }
    }

    else if (auto func = FunctionDecl::downcast(_decl)) {
      llvm::StringRef relName = func->getName()->asStringRef();
      std::string fqName = (scope + "::" + relName).str();

      if (auto collidingDecl = ont->getFunctionOrConstructor(fqName)) {
        functionNameCollisionError(fqName);
      }

      if (relName.equals("main")) {
        func->setName(fqName);
        if (!ont->entryPoint.empty()) {
          multipleEntryPointsError();
          return;
        }
        ont->recordMapName(func, "main");
      } else if (func->getID() == AST::ID::EXTERN_FUNC) {
        std::string relNameString = relName.str();
        func->setName(fqName);
        ont->recordMapName(func, relNameString);
      } else {
        func->setName(fqName);
        ont->record(func);
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
  
  void typeNameCollisionError(const std::string& repeatedType) {
    std::string errMsg = "Data type is already defined: " + repeatedType;
    errors.push_back(errMsg);
  }

  void moduleNameCollisionError(const std::string& repeatedMod) {
    std::string errMsg = "Module is already defined: " + repeatedMod;
    errors.push_back(errMsg);
  }

  void functionNameCollisionError(const std::string& repeatedFunc) {
    std::string errMsg = "Function or procedure is already defined: " + repeatedFunc;
    errors.push_back(errMsg);
  }

  void multipleEntryPointsError() {
    errors.push_back("There are multiple program entry points");
  }
};

#endif