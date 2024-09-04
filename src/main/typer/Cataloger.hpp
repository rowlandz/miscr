#ifndef TYPER_CATALOGER
#define TYPER_CATALOGER

#include "common/NodeManager.hpp"
#include "common/Ontology.hpp"

/**
 * First of three type checking phases.
 * Catalogs all decl names into an Ontology.
 */
class Cataloger {
public:

  const NodeManager* m;
  Ontology* ont;
  std::vector<std::string> errors;

  Cataloger(const NodeManager* m, Ontology* ont) {
    this->m = m;
    this->ont = ont;
  }

  void catalog(const std::string& scope, unsigned int _decl) {
    Node decl = m->get(_decl);
    
    if (decl.ty == NodeTy::MODULE) {
      Node declIdent = m->get(decl.n1);
      std::string moduleName = scope + "::" + std::string(declIdent.extra.ptr, declIdent.loc.sz);
      auto existingEntry = ont->moduleSpace.find(moduleName);
      if (existingEntry == ont->moduleSpace.end()){
        ont->moduleSpace[moduleName] = _decl;
      } else {
        moduleNameCollisionError(moduleName);
      }

      Node declList = m->get(decl.n2);
      while (declList.ty == NodeTy::DECLLIST_CONS) {
        catalog(moduleName, declList.n1);
        declList = m->get(declList.n2);
      }
    }

    else if (decl.ty == NodeTy::NAMESPACE) {
      Node declIdent = m->get(decl.n1);
      std::string namespaceName = scope + "::" + std::string(declIdent.extra.ptr, declIdent.loc.sz);
      Node declList = m->get(decl.n2);
      while (declList.ty == NodeTy::DECLLIST_CONS) {
        catalog(namespaceName, declList.n1);
        declList = m->get(declList.n2);
      }
    }

    else if (decl.ty == NodeTy::FUNC || decl.ty == NodeTy::PROC
         || decl.ty == NodeTy::EXTERN_FUNC || decl.ty == NodeTy::EXTERN_PROC) {
      Node declIdent = m->get(decl.n1);
      std::string funcName = scope + "::" + std::string(declIdent.extra.ptr, declIdent.loc.sz);
      auto existingEntry = ont->functionSpace.find(funcName);
      if (existingEntry == ont->functionSpace.end()){
        ont->functionSpace[funcName] = _decl;
      } else {
        functionNameCollisionError(funcName);
      }
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
};

#endif