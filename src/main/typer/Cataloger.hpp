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

  /** Catalog a decl.
   * `scope` is the scope that `_decl` appears in.
   */
  void catalog(const std::string& scope, unsigned int _decl) {
    Node decl = m->get(_decl);
    
    if (decl.ty == NodeTy::MODULE) {
      Node declIdent = m->get(decl.n1);
      std::string moduleName = scope + "::" + std::string(declIdent.extra.ptr, declIdent.loc.sz);
      auto existingEntry = ont->findModule(moduleName);
      if (existingEntry == ont->moduleSpaceEnd()){
        ont->recordModule(moduleName, _decl);
      } else {
        moduleNameCollisionError(moduleName);
      }

      catalogDeclList(moduleName, decl.n2);
    }

    else if (decl.ty == NodeTy::NAMESPACE) {
      Node declIdent = m->get(decl.n1);
      std::string namespaceName = scope + "::" + std::string(declIdent.extra.ptr, declIdent.loc.sz);
      catalogDeclList(namespaceName, decl.n2);
    }

    else if (decl.ty == NodeTy::FUNC || decl.ty == NodeTy::PROC
         || decl.ty == NodeTy::EXTERN_FUNC || decl.ty == NodeTy::EXTERN_PROC) {
      Node declIdent = m->get(decl.n1);
      std::string uqname = std::string(declIdent.extra.ptr, declIdent.loc.sz);
      std::string fqname = scope + "::" + uqname;

      if (uqname == "main") {
        if (!ont->entryPoint.empty()) {
          multipleEntryPointsError();
        } else {
          ont->recordMainProc(fqname, _decl);
        }
      } else {
        auto existingEntry = ont->findFunction(fqname);
        if (existingEntry == ont->functionSpaceEnd()) {
          if (decl.ty == NodeTy::EXTERN_FUNC || decl.ty == NodeTy::EXTERN_PROC)
            ont->recordExtern(fqname, uqname, _decl);
          else
            ont->recordFunction(fqname, _decl);
        } else {
          functionNameCollisionError(fqname);
        }
      }
    }
  }

  /** Catalogs the decls in a decl list. `scope` is the scope that the decls appear in. */
  void catalogDeclList(const std::string& scope, unsigned int _declList) {
    Node declList = m->get(_declList);
    while (declList.ty == NodeTy::DECLLIST_CONS) {
      catalog(scope, declList.n1);
      declList = m->get(declList.n2);
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