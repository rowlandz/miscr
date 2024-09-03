#ifndef COMMON_ONTOLOGY
#define COMMON_ONTOLOGY

#include <string>
#include <map>

class Ontology {
public:

  /** Maps the fully-qualified name of a function and procedure to its
   * declaration or definition (in a NodeManager).
   */
  std::map<std::string, unsigned int> functionSpace;

  /** Maps the fully-qualified name of a module to its
   * definition (in a NodeManager). */
  std::map<std::string, unsigned int> moduleSpace;
};

#endif