#ifndef COMMON_ONTOLOGY
#define COMMON_ONTOLOGY

#include "llvm/ADT/StringMap.h"
#include "common/AST.hpp"

/// @brief Maps the fully qualified names of all decls to their definitions in
/// the AST.
///
/// There are three distinct "spaces" of fully-qualified names (type space,
/// function space, and module space). The parser can always tell when a symbol
/// is being used as a type, function, or module, so these spaces can overlap.
class Ontology {

public:

  // TODO: make private
  llvm::StringMap<StructDecl*> typeSpace;
  llvm::StringMap<FunctionDecl*> functionSpace;
  llvm::StringMap<ModuleDecl*> moduleSpace;

  llvm::StringMap<std::string> mappedFuncNames;

  enum struct Space { FUNCTION, MODULE, TYPE };

  /// @brief The fully-qualified name of the "main" entry point procedure.
  /// Empty if no entry point has been found.
  std::string entryPoint;

  void record(llvm::StringRef fqn, StructDecl* decl)
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
    }
    llvm_unreachable("Ontology::getDecl() unhandled switch case");
    return nullptr;
  }

  /// @brief Finds a data type in the type space. Returns nullptr if not found. 
  StructDecl* getType(llvm::StringRef name) const {
    return typeSpace.lookup(name);
  }

  /// @brief Finds a function or extern function in the function space.
  /// Returns `Addr::none()` if not found.
  FunctionDecl* getFunction(llvm::StringRef name) const {
    return functionSpace.lookup(name);
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