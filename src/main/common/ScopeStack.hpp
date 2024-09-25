#ifndef COMMON_SCOPESTACK
#define COMMON_SCOPESTACK

#include <string>
#include <vector>
#include "llvm/ADT/StringMap.h"

/// Manages information about program variables.
template <typename V>
class ScopeStack {

  std::vector<llvm::StringMap<V>> scopes;
  
public:

  ScopeStack() {
    push();   // do an initial push so there is always at least one scope.
  }

  /** Adds a variable to the topmost scope. */
  void add(llvm::StringRef varName, V addr) {
    scopes.back()[varName] = addr;
  }

  /// Finds `varName` in the scopes stack. If multiple scopes contain `varName`,
  /// then the occurrance from the most recently pushed scope is used. Returns
  /// `alt` if the var does not exist in any scope.
  V getOrElse(llvm::StringRef varName, V alt) {
    for (auto scope = scopes.crbegin(); scope < scopes.crend(); ++scope) {
      auto result = scope->find(varName);
      if (result != scope->end()) return result->second;
    }
    return alt;
  }

  /// Pushs a new empty scope onto the stack.
  void push() {
    scopes.push_back(llvm::StringMap<V>());
  }

  /// Pops a scope off the stack.
  void pop() {
    scopes.pop_back();
  }
};

#endif