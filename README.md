# Minimalist Safe C Replacement (MiSCR)

Nice straight-to-the-point tutorial using the LLVM C API:
https://www.pauladamsmith.com/blog/2015/01/how-to-get-started-with-llvm-c-api.html

Cool tutorial
https://mukulrathi.com/create-your-own-programming-language/llvm-ir-cpp-api-tutorial/

C++ Core Documentation
https://llvm.org/doxygen/group__LLVMCCore.html

Minimal Readline replacement:
https://github.com/antirez/linenoise?tab=BSD-2-Clause-1-ov-file


Minimal dependencies:
* LLVM C++ libraries
* Clang++ compiler
* Bash
* (optional) GNU make
Dead-simple unit test infrastructure; The include file `test.hpp` is only 50 lines of code!


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

MiSCR is _not_ object oriented for the following reasons. A "class" is just
a namespace combined with a data type. Classes have some frustrating
pain-points that other languages are often forced to work around:
* Other code cannot add functions to the class even when it makes sense
  to do so.
* Multiple invocation syntaxes that are not interchangeable.
  * e.g., `myobj.mymethod(param1)` versus `mymethod(myobj, param1)`.
* Often unclear whether a function _mutates_ the object or returns a modified _copy_.
  * e.g., `mylist.sort()` versus `mylist.sorted()`

MiSCR claims that there is no reason to combine namespaces and data types into this concept of a "class". They just exist as separate things.

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

# Arrays and References

```
              Ampersand means "read-only reference"
&i32          read-only reference to an i32
&x            read-only reference to x

              Hash means "writable reference"
#i32
#x

              Consistent array notation
[10 of i32]   type of an 10-length array of i32s
[10 of 0]     a literal array of length 10 filled with zeros

              Postfix "." always means a projection function
[10 of 0].3   The value of the array at index 3 (having type i32)
mypos.x       The x field of the struct

              Postfix `!` means plain dereference
arrRef!       The array pointed to by `arrRef`
myposRef!     The (whole) struct pointed to by `myposRef`

              Postfix brackets mean address pointer calculation
arrRef[2]     The address of the index-2 element of the array pointed to by arrRef
mypos[x]      The address of the x field of the struct referenced by mypos

              Postfix "->" always means projection _through a reference_
arrRef->3     The value at index 3 of the array referred to by `arrRef`
myposRef->x   The x field of the struct pointed to by `myposRef`


Dereference and address calculation operators are all postfix.

Creating references are all prefix.



func foo() {
  let x: #&BigData = ...;
  x := &differentBigData;
  x! := BigData();           // illegal store via read-only reference
}

```

# Data Structures

A simple C-like structure (i.e., a block of memory divided into fields).

```
data Person(name: &str, age: i32)

let bob: Person = Person("Bob", 40);
let bobsage: i32 = bob.age;
```

An abstract data structure expects other data structures to extend it with
more fields. Therefore it cannot be instantiated directly. The size of an
abstract data structure is equal to the size of its largest descendant.

```
abstract data AST(loc: Location)

abstract data Exp(extends AST, ty: Type)

data IntLit(extends Exp, value: i64)

data Add(extends Exp, lhs: &Exp, rhs: &Exp)
```

The address of a field can be calculated from the address of a struct:

```
let bob: &Person("Bob", 40);
let bobsage: &i32 = bob[age];
```

## Reference Safety: `borrow` and `move`

`borrow` and `move` are two operators with these types:

    borrow : forall t . #t -> &t
    move   : forall t . &#t -> #t

`borrow` lets you make copies of an owned reference with the understanding that
the copies will not be freed, only the original owned reference.

`move` lets you move an owned pointer out of a place in memory. However, you
must replace the pointer with another owned pointer later.

move p
// cannot dereference p
p := anotherOwnedPointer

## Notes:

Use `clang -mllvm -opaque-pointers` to compile with clang version 14.

TODO: Make a better error for when you use a name that has no binding.
e.g.,

    data String(ptr: #i8, len: i64)
    func foo(p: #i8): String = {
      String(ptr, 10)
    };