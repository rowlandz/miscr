#ifndef TYPER_CATALOGER
#define TYPER_CATALOGER

#include "common/AST.hpp"
#include "common/ASTContext.hpp"
#include "common/Ontology.hpp"

/// @brief First of the type checking phases. Fully qualifies all decl names
/// and builds an `Ontology`.
class Cataloger {
public:

  ASTContext* ctx;
  Ontology* ont;
  std::vector<std::string> errors;

  Cataloger(ASTContext* ctx, Ontology* ont) {
    this->ctx = ctx;
    this->ont = ont;
  }

  /// Catalogs a decl. `scope` is the scope that `_decl` appears in.
  void catalog(const std::string& scope, Addr<Decl> _decl) {
    AST::ID id = ctx->get(_decl).getID();

    if (id == AST::ID::MODULE) {
      Addr<ModuleDecl> _module = _decl.UNSAFE_CAST<ModuleDecl>();
      ModuleDecl module = ctx->get(_module);
      Ident relName = ctx->GET_UNSAFE<Ident>(module.getName());
      std::string fqNameStr = scope + "::" + relName.asString();

      if (ont->getModule(fqNameStr).isError()) {
        FQNameKey key = ont->recordModule(fqNameStr, _module);
        Addr<FQIdent> fqName = ctx->add(FQIdent(relName.getLocation(), key));
        module.setName(fqName);
        ctx->set(_module, module);
      } else {
        moduleNameCollisionError(fqNameStr);
      }

      catalogDeclList(fqNameStr, module.getDecls());
    }

    // else if (id == AST::ID::NAMESPACE) {
    //   Addr<NamespaceDecl> _ns = _decl.UNSAFE_CAST<NamespaceDecl>();
    //   NamespaceDecl ns = ctx->get(_ns);
    //   Ident relName = ctx->GET_UNSAFE<Ident>(ns.getName());
    //   std::string fqNameStr = scope + "::" + relName.asString();
    //   catalogDeclList(fqNameStr, ns.getDecls());
    // }

    else if (id == AST::ID::FUNC || id == AST::ID::EXTERN_FUNC) {
      Addr<FunctionDecl> _funcDecl = _decl.UNSAFE_CAST<FunctionDecl>();
      FunctionDecl funcDecl = ctx->get(_funcDecl);
      Ident relName = ctx->GET_UNSAFE<Ident>(funcDecl.getName());
      std::string relNameStr = relName.asString();
      std::string fqNameStr = scope + "::" + relNameStr;

      if (ont->getFunctionDecl(fqNameStr).exists()) {
        functionNameCollisionError(fqNameStr);
        return;
      }

      if (relNameStr == "main") {
        if (!ont->entryPoint.empty()) {
          multipleEntryPointsError();
          return;
        }
        ont->recordMainProc(fqNameStr, _funcDecl);
      } else {
        FQNameKey key = id == AST::ID::EXTERN_FUNC ?
                        ont->recordExtern(fqNameStr, relNameStr, _funcDecl) :
                        ont->recordFunction(fqNameStr, _funcDecl);
        Addr<FQIdent> fqName = ctx->add(FQIdent(relName.getLocation(), key));
        funcDecl.setName(fqName);
        ctx->set(_funcDecl, funcDecl);
      }
    }
  }

  /// Catalogs the decls in a decl list. `scope` is the scope that the decls
  /// appear in.
  void catalogDeclList(const std::string& scope, Addr<DeclList> _declList) {
    DeclList declList = ctx->get(_declList);
    while (declList.getID() == AST::ID::DECLLIST_CONS) {
      catalog(scope, declList.getHead());
      declList = ctx->get(declList.getTail());
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