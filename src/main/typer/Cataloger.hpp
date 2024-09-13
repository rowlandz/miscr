#ifndef TYPER_CATALOGER
#define TYPER_CATALOGER

#include "common/AST.hpp"
#include "common/ASTContext.hpp"
#include "common/Ontology.hpp"

/**
 * First of three type checking phases.
 * Catalogs all decl names into an Ontology.
 */
class Cataloger {
public:

  const ASTContext* m;
  Ontology* ont;
  std::vector<std::string> errors;

  Cataloger(const ASTContext* m, Ontology* ont) {
    this->m = m;
    this->ont = ont;
  }

  /** Catalog a decl.
   * `scope` is the scope that `_decl` appears in.
   */
  void catalog(const std::string& scope, Addr<Decl> _decl) {
    ASTMemoryBlock declBlock = m->get(_decl.UNSAFE_CAST<ASTMemoryBlock>());
    AST* ast = reinterpret_cast<AST*>(&declBlock);

    if (ast->getID() == AST::ID::MODULE) {
      ModuleDecl* mod = reinterpret_cast<ModuleDecl*>(&declBlock);
      Ident declName = m->get(mod->getName().UNSAFE_CAST<Ident>());
      std::string moduleName = scope + "::" + declName.asString();
      auto existingEntry = ont->findModule(moduleName);
      if (existingEntry == ont->moduleSpaceEnd()){
        ont->recordModule(moduleName, _decl.UNSAFE_CAST<ModuleDecl>());
      } else {
        moduleNameCollisionError(moduleName);
      }

      catalogDeclList(moduleName, mod->getDecls());
    }

    else if (ast->getID() == AST::ID::NAMESPACE) {
      NamespaceDecl* ns = reinterpret_cast<NamespaceDecl*>(&declBlock);
      Ident declName = m->get(ns->getName().UNSAFE_CAST<Ident>());
      std::string namespaceName = scope + "::" + declName.asString();
      catalogDeclList(namespaceName, ns->getDecls());
    }

    else if (ast->getID() == AST::ID::FUNC
          || ast->getID() == AST::ID::EXTERN_FUNC) {
      FunctionDecl* func = reinterpret_cast<FunctionDecl*>(&declBlock);
      Ident declName = m->get(func->getName().UNSAFE_CAST<Ident>());
      std::string uqname = declName.asString();
      std::string fqname = scope + "::" + uqname;

      if (uqname == "main") {
        if (!ont->entryPoint.empty()) {
          multipleEntryPointsError();
        } else {
          ont->recordMainProc(fqname, _decl.UNSAFE_CAST<FunctionDecl>());
        }
      } else {
        auto existingEntry = ont->findFunction(fqname);
        if (existingEntry == ont->functionSpaceEnd()) {
          if (func->getID() == AST::ID::EXTERN_FUNC)
            ont->recordExtern(fqname, uqname, _decl.UNSAFE_CAST<FunctionDecl>());
          else
            ont->recordFunction(fqname, _decl.UNSAFE_CAST<FunctionDecl>());
        } else {
          functionNameCollisionError(fqname);
        }
      }
    }
  }

  /// Catalogs the decls in a decl list. `scope` is the scope that the decls
  /// appear in.
  void catalogDeclList(const std::string& scope, Addr<DeclList> _declList) {
    DeclList declList = m->get(_declList);
    while (declList.getID() == AST::ID::DECLLIST_CONS) {
      catalog(scope, declList.getHead());
      declList = m->get(declList.getTail());
    }
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