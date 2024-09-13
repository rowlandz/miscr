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

  std::map<std::string, const char*> permaPointers;

  /** Maps the fully-qualified name of a function and procedure to its
   * declaration or definition (in an ASTContext). */
  std::map<const char*, Addr<FunctionDecl>> functionSpace;

  /** Maps the fully-qualified name of a module to its
   * definition (in a NodeManager). */
  std::map<const char*, Addr<ModuleDecl>> moduleSpace;

public:

  /** The fully-qualified name of the "main" entry point procedure.
   * Empty if no entry point has been found. */
  std::string entryPoint;
  
  ~Ontology() {
    for (auto pair : permaPointers) {
      free((void*)pair.second);
    }
  }

  /// Records an entry for a func named `name` with definition at `declAddr`.
  /// Note: this does *not* handle externs; use `recordExtern` instead.
  void recordFunction(std::string name, Addr<FunctionDecl> declAddr) {
    const char* ptr = recordNewPermaPointer(name, name);
    functionSpace[ptr] = declAddr;
  }

  /// Records an entry for an extern func named `name` with declaration at
  /// `declAddr`.
  void recordExtern(std::string fqname, std::string shortName,
      Addr<FunctionDecl> declAddr) {
    const char* ptr = recordNewPermaPointer(fqname, shortName);
    functionSpace[ptr] = declAddr;
  }

  /// Records an entry for the main function/procedure.
  /// Functionally the same as `recordExtern(fqname, "main", declAddr)`.
  void recordMainProc(std::string fqname, Addr<FunctionDecl> declAddr) {
    recordExtern(fqname, "main", declAddr);
  }

  /// Records an entry for a module named `name` with definition at `declAddr`.
  void recordModule(std::string name, Addr<ModuleDecl> declAddr) {
    const char* ptr = recordNewPermaPointer(name, name);
    moduleSpace[ptr] = declAddr;
  }

  /// Finds a func or extern func in the function space.
  std::map<const char*, Addr<FunctionDecl>>::const_iterator
  findFunction(const std::string& name) const {
    auto p = permaPointers.find(name);
    if (p == permaPointers.end()) return functionSpace.end();
    else return functionSpace.find(p->second);
  }

  std::map<const char*, Addr<ModuleDecl>>::const_iterator
  findModule(const std::string& name) const {
    auto p = permaPointers.find(name);
    if (p == permaPointers.end()) return moduleSpace.end();
    else return moduleSpace.find(p->second);
  }

  std::map<const char*, Addr<FunctionDecl>>::const_iterator
  functionSpaceEnd() const {
    return functionSpace.end();
  }

  std::map<const char*, Addr<ModuleDecl>>::const_iterator
  moduleSpaceEnd() const {
    return moduleSpace.end();
  }

  /// Returns a pointer to a C-string equivalent to `name` that
  /// won't change as long as this Ontology object still exists.
  const char* getPermaPointer(std::string& name) const {
    auto ptr = permaPointers.at(name);
    return ptr;
  }

private:

  const char* recordNewPermaPointer(std::string fqname, std::string toStore) {
    char* newPtr = (char*)malloc(toStore.size() + 1);
    strcpy(newPtr, toStore.c_str());
    newPtr[toStore.size()] = '\0';
    permaPointers[fqname] = newPtr;
    return newPtr;
  }
};

#endif