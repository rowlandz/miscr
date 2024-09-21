#ifndef COMMON_ONTOLOGY
#define COMMON_ONTOLOGY

#include <string>
#include <map>
#include <cstring>
#include "common/ASTContext.hpp"
#include "common/AST.hpp"

/// Holds the fully qualified names of all decls and maps them to their
/// definitions in an `ASTContext`. An ontology is produced by the `Cataloger`
/// phase of typechecking and should not be modified thereafter.
///
/// There are three distinct "spaces" of fully-qualified names (type space,
/// function space, and module space). The type space and function space must
/// be disjoint since every data type also has a constructor function of the
/// same name. However, either can overlap with the module space.
///
/// The decls and `FQNameKey`s are in one-to-one correspondence.
class Ontology {

  /// @brief Maps the fully-qualified name of each `data` decl to a unique key.
  std::map<std::string, FQNameKey> typeKeys;

  /// @brief Maps the fully-qualified name of each function to a unique key.
  std::map<std::string, FQNameKey> functionKeys;

  /// @brief Maps the fully-qualified name of each module to a unique key.
  std::map<std::string, FQNameKey> moduleKeys;

  /// @brief Maps `FQNameKey` to function names (as they appear in LLVM IR).
  std::vector<std::string> names;

  /// @brief Maps `FQNameKey` to the decls.
  std::vector<Addr<Decl>> decls;

public:

  /// The fully-qualified name of the "main" entry point procedure.
  /// Empty if no entry point has been found.
  std::string entryPoint;

  /// Records an entry for a data type and returns the freshly generated key
  /// for this data type. 
  FQNameKey recordType(std::string name, Addr<DataDecl> _decl) {
    return record(name, name, _decl.upcast<Decl>(), typeKeys);
  }

  /// Records an entry for a func named `name` with definition at `_addr`.
  /// Note: this does *not* handle externs; use `recordExtern` instead.
  FQNameKey recordFunction(std::string name, Addr<FunctionDecl> _decl) {
    return record(name, name, _decl.upcast<Decl>(), functionKeys);
  }

  /// Records an entry for an extern func named `name` with declaration at
  /// `_decl`.
  FQNameKey recordExtern(std::string fqname, std::string shortName,
        Addr<FunctionDecl> _decl) {
    return record(fqname, shortName, _decl.upcast<Decl>(), functionKeys);
  }

  /// Records an entry for the main function/procedure.
  /// Functionally the same as `recordExtern(fqname, "main", declAddr)`.
  FQNameKey recordMainProc(std::string fqname, Addr<FunctionDecl> declAddr) {
    return recordExtern(fqname, "main", declAddr);
  }

  /// Records an entry for a module named `name` with definition at `declAddr`.
  FQNameKey recordModule(std::string name, Addr<ModuleDecl> _decl) {
    return record(name, name, _decl.upcast<Decl>(), moduleKeys);
  }



  /// @brief Finds a data type in the type space. Returns error if not found. 
  Addr<DataDecl> getTypeDecl(std::string name) const {
    auto result = functionKeys.find(name);
    if (result == typeKeys.end()) return Addr<DataDecl>::none();
    return decls[result->second.getValue()].UNSAFE_CAST<DataDecl>();
  }

  /// @brief Finds a function or extern function in the function space.
  /// Returns `Addr::none()` if not found.
  Addr<FunctionDecl> getFunctionDecl(std::string name) const {
    auto result = functionKeys.find(name);
    if (result == functionKeys.end()) return Addr<FunctionDecl>::none();
    return decls[result->second.getValue()].UNSAFE_CAST<FunctionDecl>();
  }

  /// @brief Looks for a `FunctionDecl` in the function space or a `DataDecl`
  /// in the type space corresponding to `name`.
  Addr<Decl> getFunctionOrConstructor(std::string name) const {
    std::map<std::string, FQNameKey>::const_iterator result;
    if ((result = functionKeys.find(name)) != functionKeys.end())
      return decls[result->second.getValue()];
    if ((result = typeKeys.find(name)) != typeKeys.end())
      return decls[result->second.getValue()];
    return Addr<Decl>::none();
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

private:
  /// Generates a fresh `FQNameKey` for `name`. This key can be used later to
  /// lookup the `shortName` and/or `_decl`. Also, `name` and `space` can be
  /// used to lookup this key.
  FQNameKey record(std::string name, std::string shortName, Addr<Decl> _decl,
      std::map<std::string, FQNameKey>& space) {
    FQNameKey ret(names.size());
    space[name] = ret;
    names.push_back(shortName);
    decls.push_back(_decl);
    return ret;
  }
};

#endif