#ifndef COMMON_ASTCONTEXT
#define COMMON_ASTCONTEXT

#include <vector>

/// @brief The address of an AST element in an ASTContext.
template <class T>
class Addr {
public:
  unsigned int addr;     // TODO: make this private

  static Addr none() { return Addr(0xffffffff); }

  Addr() { addr = 0xffffffff; }
  Addr(unsigned int addr) { this->addr = addr; }

  /// Returns true if this address is invalid.
  bool isError() const { return addr >= 0xfffffffe; }

  /// Opposite of `isError`. 
  bool exists() const { return addr < 0xfffffffe; }

  /// @brief Epsilon is the special error address 0xfffffffe.
  bool isEpsilon() const { return addr == 0xfffffffe; }

  /// @brief Opposite of `isEpsilon` 
  bool notEpsilon() const { return addr != 0xfffffffe; }

  /// @brief Arrest is the special error address 0xffffffff.
  bool isArrest() const { return addr == 0xffffffff; }

  /// @brief Cast the address of a derived type to the address of a base type.
  template<class U>
  std::enable_if_t<std::is_base_of<U,T>::value, Addr<U> >
  upcast() const { return Addr<U>(addr); }

  /// @brief Cast the pointee type to any other type.
  template<class U> Addr<U> UNSAFE_CAST() const { return Addr<U>(addr); }
};

/// @brief A helper class that can be implicitly cast to any `Addr` type.
class AnyAddr {
  unsigned int addr;
public:
  AnyAddr(unsigned int addr) { this->addr = addr; }
  template<class U> operator Addr<U>() { return Addr<U>(addr); }
};

static AnyAddr EPSILON_ERROR(0xfffffffe);
static AnyAddr ARRESTING_ERROR(0xffffffff);

/// @brief Defines a block of memory whose size is at least as big as the
/// biggest AST element for memory allocation purposes.
class ASTMemoryBlock {
  unsigned long unused0 : 64;
  unsigned long unused1 : 64;
  unsigned long unused2 : 64;
  unsigned long unused3 : 64;
};

/// @brief A context for managing AST elements.
class ASTContext {
  std::vector<ASTMemoryBlock> nodes;

public:
  template<class T>
  T get(Addr<T> addr) const {
    ASTMemoryBlock ret = nodes[addr.addr];
    return *reinterpret_cast<T*>(&ret);
  }

  /// @brief Unsafely casts `addr` to an `T` address and retrieves the data. 
  template<class T, class U>
  T GET_UNSAFE(Addr<U> addr) const {
    ASTMemoryBlock ret = nodes[addr.addr];
    return *reinterpret_cast<T*>(&ret);
  }

  template<class T>
  const T* getConstPtr(Addr<T> addr) const {
    return reinterpret_cast<const T*>(nodes.begin().base() + addr.addr);
  }

  template <class T>
  T operator[](Addr<T> addr) const {
    return static_cast<T>(this->nodes[addr.addr]);
  }

  template<class T>
  void set(Addr<T> addr, T node) {
    nodes[addr.addr] = *reinterpret_cast<ASTMemoryBlock*>(&node);
  }

  template<class T, class U>
  void SET_UNSAFE(Addr<U> addr, T node) {
    nodes[addr.addr] = *reinterpret_cast<ASTMemoryBlock*>(&node);
  }

  template<class T>
  Addr<T> add(T node) {
    ASTMemoryBlock* block = reinterpret_cast<ASTMemoryBlock*>(&node);
    nodes.push_back(*block);
    return Addr<T>(nodes.size() - 1);
  }

  template<class T>
  bool isValid(Addr<T> a) const { return a.addr < nodes.size(); }
};

#endif