#ifndef COMMON_ONTOLOGY
#define COMMON_ONTOLOGY

#include <string>
#include <map>
#include <cstring>
#include "common/ASTContext.hpp"
#include "common/AST.hpp"

/**
 * Holds the fully qualified names of all decls and maps them to their
 * definitions in a NodeManager. An ontology is produced by the Cataloger
 * phase of typechecking and should not be modified thereafter.
 * 
 * For extern decls, the fully-qualified name is used to look up the decl,
 * but the name returned will be the unqualified name.
 * 
 * The entry point function (like externs) is also stored by its unqualified
 * name "main".
 */
class Ontology {

  std::map<std::string, FQNameKey> functionKeys;

  std::map<std::string, FQNameKey> moduleKeys;

  /// @brief Maps `FQNameKey` to function names (as they appear in LLVM IR).
  std::vector<std::string> names;

  /// @brief Maps `FQNameKey` to the decls.
  std::vector<Addr<Decl>> decls;

public:

  /** The fully-qualified name of the "main" entry point procedure.
   * Empty if no entry point has been found. */
  std::string entryPoint;

  /// Records an entry for a func named `name` with definition at `declAddr`.
  /// Note: this does *not* handle externs; use `recordExtern` instead.
  FQNameKey recordFunction(std::string name, Addr<FunctionDecl> declAddr) {
    FQNameKey ret(names.size());
    functionKeys[name] = ret;
    names.push_back(name);
    decls.push_back(declAddr.upcast<Decl>());
    return ret;
  }

  /// Records an entry for an extern func named `name` with declaration at
  /// `declAddr`.
  FQNameKey recordExtern(std::string fqname, std::string shortName,
        Addr<FunctionDecl> declAddr) {
    FQNameKey ret(names.size());
    functionKeys[fqname] = ret;
    names.push_back(shortName);
    decls.push_back(declAddr.upcast<Decl>());
    return ret;
  }

  /// Records an entry for the main function/procedure.
  /// Functionally the same as `recordExtern(fqname, "main", declAddr)`.
  void recordMainProc(std::string fqname, Addr<FunctionDecl> declAddr) {
    recordExtern(fqname, "main", declAddr);
  }

  /// Records an entry for a module named `name` with definition at `declAddr`.
  FQNameKey recordModule(std::string name, Addr<ModuleDecl> _decl) {
    FQNameKey ret(names.size());
    moduleKeys[name] = ret;
    names.push_back(name);
    decls.push_back(_decl.upcast<Decl>());
    return ret;
  }

  /// @brief Finds a function or extern function in the function space.
  /// Returns `Addr::none()` if not found.
  Addr<FunctionDecl> getFunctionDecl(std::string name) const {
    auto result = functionKeys.find(name);
    if (result == functionKeys.end()) return Addr<FunctionDecl>::none();
    return decls[result->second.getValue()].UNSAFE_CAST<FunctionDecl>();
  }

  /// @brief Finds a module in the module space. Returns `Addr::none` if
  /// not found.
  Addr<ModuleDecl> getModule(std::string name) const {
    auto result = moduleKeys.find(name);
    if (result == moduleKeys.end()) return Addr<ModuleDecl>::none();
    return decls[result->second.getValue()].UNSAFE_CAST<ModuleDecl>();
  }

  /// @brief Lookup the name associated with this key. 
  std::string getName(FQNameKey key) const {
    return names[key.getValue()];
  }
};

#endif