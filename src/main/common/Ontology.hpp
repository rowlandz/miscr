#ifndef COMMON_ONTOLOGY
#define COMMON_ONTOLOGY

#include <string>
#include <map>

class Ontology {
public:
  
  ~Ontology() {
    for (auto pair : permaPointers) {
      free((void*)pair.second);
    }
  }

  void recordFunction(std::string name, unsigned int declAddr) {
    const char* ptr = recordNewPermaPointer(name);
    functionSpace[ptr] = declAddr;
  }

  void recordModule(std::string name, unsigned int declAddr) {
    const char* ptr = recordNewPermaPointer(name);
    moduleSpace[ptr] = declAddr;
  }

  std::map<const char*, unsigned int>::const_iterator findFunction(const std::string& name) const {
    auto p = permaPointers.find(name);
    if (p == permaPointers.end()) return functionSpace.end();
    else return functionSpace.find(p->second);
  }

  std::map<const char*, unsigned int>::const_iterator findModule(const std::string& name) const {
    auto p = permaPointers.find(name);
    if (p == permaPointers.end()) return moduleSpace.end();
    else return moduleSpace.find(p->second);
  }

  std::map<const char*, unsigned int>::const_iterator functionSpaceEnd() const {
    return functionSpace.end();
  }

  std::map<const char*, unsigned int>::const_iterator moduleSpaceEnd() const {
    return moduleSpace.end();
  }

  /** Returns a pointer to a C-string equivalent to `name` that
   * won't change as long as this Ontology object still exists. */
  const char* getPermaPointer(std::string& name) const {
    auto ptr = permaPointers.at(name);
    return ptr;
  }

private:

  std::map<std::string, const char*> permaPointers;

  /** Maps the fully-qualified name of a function and procedure to its
   * declaration or definition (in a NodeManager). */
  std::map<const char*, unsigned int> functionSpace;

  /** Maps the fully-qualified name of a module to its
   * definition (in a NodeManager). */
  std::map<const char*, unsigned int> moduleSpace;

  const char* recordNewPermaPointer(std::string name) {
    char* newPtr = (char*)malloc(name.size() + 1);
    strcpy(newPtr, name.c_str());
    newPtr[name.size()] = '\0';
    permaPointers[name] = newPtr;
    return newPtr;
  }
};

#endif