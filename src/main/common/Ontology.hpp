#ifndef COMMON_ONTOLOGY
#define COMMON_ONTOLOGY

#include "llvm/ADT/StringMap.h"
#include "common/AST.hpp"

/// Maps the fully qualified names of all decls to their definitions in the AST.
///
/// There are three distinct "spaces" of fully-qualified names (type space,
/// function space, and module space). The type space and function space must
/// be disjoint since every data type also has a constructor function of the
/// same name. However, either can overlap with the module space.
class Ontology {

public:
  llvm::StringMap<DataDecl*> typeSpace;
  llvm::StringMap<FunctionDecl*> functionSpace;
  llvm::StringMap<ModuleDecl*> moduleSpace;

  llvm::StringMap<std::string> mappedFuncNames;

  enum struct Space { FUNCTION, MODULE, TYPE, FUNCTION_OR_TYPE };

  /// The fully-qualified name of the "main" entry point procedure.
  /// Empty if no entry point has been found.
  std::string entryPoint;

  void record(llvm::StringRef fqn, DataDecl* decl)
    { typeSpace[fqn] = decl; }
  void record(llvm::StringRef fqn, ModuleDecl* decl)
    { moduleSpace[fqn] = decl; }
  void record(llvm::StringRef fqn, FunctionDecl* decl)
    { functionSpace[fqn] = decl; }

  void recordMapName(llvm::StringRef fqn, FunctionDecl* decl,
      llvm::StringRef mappedName) {
    functionSpace[fqn] = decl;
    mappedFuncNames[fqn] = mappedName.str();
  }

  /// @brief Looks for a declaration with `name` in the specified `space`.
  Decl* getDecl(llvm::StringRef name, Space space) const {
    switch (space) {
    case Space::FUNCTION: return functionSpace.lookup(name);
    case Space::MODULE: return moduleSpace.lookup(name);
    case Space::TYPE: return typeSpace.lookup(name);
    case Space::FUNCTION_OR_TYPE:
      if (auto ret = functionSpace.lookup(name)) return ret;
      return typeSpace.lookup(name);
    }
  }

  /// @brief Finds a data type in the type space. Returns nullptr if not found. 
  DataDecl* getType(llvm::StringRef name) const {
    return typeSpace.lookup(name);
  }

  /// @brief Finds a function or extern function in the function space.
  /// Returns `Addr::none()` if not found.
  FunctionDecl* getFunction(llvm::StringRef name) const {
    return functionSpace.lookup(name);
  }

  /// @brief Looks for a `FunctionDecl` in the function space or a `DataDecl`
  /// in the type space corresponding to `name`.
  Decl* getFunctionOrConstructor(llvm::StringRef name) const {
    auto res1 = functionSpace.lookup(name);
    if (res1 != nullptr) return res1;
    auto res2 = typeSpace.lookup(name);
    if (res2 != nullptr) return res2;
    return nullptr;
  }

  /// @brief Looks for a module in the module space with the given name.
  ModuleDecl* getModule(llvm::StringRef name) const {
    return moduleSpace.lookup(name);
  }

  /// @brief Returns the mapped version of `name` if it exists, otherwise
  /// just returns `name` itself.
  llvm::StringRef mapName(llvm::StringRef name) const {
    auto res = mappedFuncNames.find(name);
    if (res == mappedFuncNames.end()) return name;
    else return res->second;
  }

};

#endif