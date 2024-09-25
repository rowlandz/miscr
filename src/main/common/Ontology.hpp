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

  llvm::StringMap<DataDecl*> typeSpace;
  llvm::StringMap<FunctionDecl*> functionSpace;
  llvm::StringMap<ModuleDecl*> moduleSpace;

  llvm::StringMap<std::string> mappedFuncNames;

public:

  /// The fully-qualified name of the "main" entry point procedure.
  /// Empty if no entry point has been found.
  std::string entryPoint;

  void record(DataDecl* decl) {
    typeSpace[decl->getName()->asStringRef()] = decl;
  }

  void record(ModuleDecl* decl) {
    moduleSpace[decl->getName()->asStringRef()] = decl;
  }

  void record(FunctionDecl* decl) {
    functionSpace[decl->getName()->asStringRef()] = decl;
  }

  void recordMapName(FunctionDecl* decl, llvm::StringRef mappedName) {
    llvm::StringRef declName = decl->getName()->asStringRef();
    functionSpace[declName] = decl;
    mappedFuncNames[declName] = mappedName.str();
  }



  /// @brief Finds a data type in the type space. Returns error if not found. 
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