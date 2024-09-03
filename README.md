# Minimalist Intuitive Safe C Replacement (MISCR)

Nice straight-to-the-point tutorial using the LLVM C API:
https://www.pauladamsmith.com/blog/2015/01/how-to-get-started-with-llvm-c-api.html

Cool tutorial
https://mukulrathi.com/create-your-own-programming-language/llvm-ir-cpp-api-tutorial/

C++ Core Documentation
https://llvm.org/doxygen/group__LLVMCCore.html


Minimal dependencies:
* LLVM C++ libraries
* Clang++ compiler
* Bash
* (optional) GNU make
Dead-simple unit test infrastructure; The include file `test.hpp` is only 54 lines of code!

TODO:
* Error handling
* Module system
* Type checker
* Multi-line input on playground


## Module System

Namespaces and modules are both collections of decls.
A decl is a func, proc, extern func, extern proc, data type, module, or namespace.
Thus, namespaces and modules form a scope tree.

```
namespace CoolMath {
  data Fraction {
    num: i32,
    den: i32,
  };
  func add(x: i32, y: i32): i32 = x + y;
  func double(x: i32): i32 = add(x, x);
  ...
}

module Foo {

}
```

The root of the scope tree is a namespace called `global`.

Decls can be accessed via their full path, which begins with `global`
(global::CoolMath::add), or a path relative to the "current scope".
(e.g., `add` from within `CoolMath` or `CoolMath::add` from within `Foo`).

If the contents of an entire file are included in a namespace or module,
then a namespace declaration can be used. The namespace declaration must
be the first non-comment thing in the file:

```
namespace CoolMath;

// rest of file
```

Omitting the namespace or module declaration is the same as using `namespace global;`

MISCR is _not_ object oriented for the following reasons. A "class" is just
a namespace combined with a data type. Classes have some frustrating
pain-points that other languages are often forced to work around:
* Other code cannot add functions to the class even when it makes sense
  to do so.
* Multiple invocation syntaxes that are not interchangeable.
  * e.g., `myobj.mymethod(param1)` versus `mymethod(myobj, param1)`.
* Often unclear whether a function _mutates_ the object or returns a modified _copy_.
  * e.g., `mylist.sort()` versus `mylist.sorted()`

MISCR claims that there is no reason to combine namespaces and data types into this concept of a "class". They just exist as separate things.

So here's the rule for method-call notation. If there is a namespace or module with the same name as a data type (e.g., `global::Color`), then:

`mycolor.lighten(degree)` is the same as `Color::lighten(mycolor, degree)`.

Here is an example of a `Person` object with full encapsulation:

```
module Person;

data T = private T {
  name: string,
  age: i32,
};

func mk(name: string, age: i32): Option<T> = {
  if age > 130 then None else Some(T{ name, age });
};

func getName(self: T): string = self.name;

func getAge(self: T): string = self.age;
```

A namespaces are _open_ and modules are _closed_.

```
namespace Pair {

  data T<A,B> = T(A,B);

  func mapFst<A,B,C>(self: T<A,B>, f: A => C): T<C,B> =
    self match {
      case T(a, b) => T(f(a), b);
    };

}
```

It is perfectly natural for other code to extend this namespace with
functions they would find useful:

```
namespace Pair {
  func mapBoth<A,B,C,D>(self: T<A,B>, f: A => C, g: B => D): T<C,D> =
    self match {
      case T(a, b) => T(f(a), g(b));
    };
}
```

But you cannot do that with a module.

## Translation Units

module A {
  func f(x: i32): i32 = x + 1;
}

module B {
  func g(x: i32): i32 = C::h();

  module C {
    func h(x: i32): i32 = x + 3;
  }
}

