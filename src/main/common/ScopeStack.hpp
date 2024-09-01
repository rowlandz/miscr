#ifndef VARSTACK
#define VARSTACK

#include <string>
#include <vector>
#include <map>

/** Manages information about program variables. */
template <typename V>
class ScopeStack {
public:

  ScopeStack() {
    push();   // do an initial push so there is always at least one scope.
  }

  /** Adds a variable to the topmost scope. */
  void add(std::string varName, V addr) {
    scopes.back()[varName] = addr;
  }

  /** Finds `varName` in the scopes stack. If multiple scopes contain `varName`, then
   * the occurrance from the most recently pushed scope is used. Returns `alt` if the
   * var does not exist in any scope. */
  V getOrElse(std::string &varName, V alt) {
    for (auto scope = scopes.end()-1; scopes.begin() <= scope; scope--) {
      auto result = scope->find(varName);
      if (result != scope->end()) {
        return result->second;
      }
    }
    return alt;
  }

  /** Pushs a new empty scope onto the stack. */
  void push() {
    scopes.push_back(std::map<std::string, V>());
  }

  /** Pops a scope off the stack. */
  void pop() {
    scopes.pop_back();
  }

private:
  std::vector<std::map<std::string, V>> scopes;
};

#endif