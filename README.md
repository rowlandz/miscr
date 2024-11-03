# Minimalist Safe C Replacement (MiSCR)

An aspiring replacement for C/C++ with a minimalist design and pointer safety
powered by a borrow checker.

## Build

Minimalist includes minimal dependencies. All you need is
  - LLVM-14 C++ libraries
  - Clang++ compiler
  - Bash
  - A few common Unix utilities (echo, rm, basename)
  - (optional) GNU `make` -- alternative to running the build script directly.

There's three executable you can build:
  - `compiler` -- The MiSCR compiler.
  - `tests` -- Automated unit tests.
  - `playground` -- An interactive testing tool.

Just run `build.sh` with the name of the executable you want to build. For
example, to build the compiler:

```shell
./build.sh compiler
```

If you ever need help, just run the build script with no arguments to get a
help message:

```shell
./build.sh
```

You can also use `make` to run the build script instead of running it
directly:

```shell
make compiler
```

## Unit Testing

MiSCR uses a dead-simple test infrastructure of only 50 lines of code found
in `src/test/test.hpp`. Building and running tests is this simple:

```shell
./build.sh tests
./tests
```

## Playground

If you want to hack around with the components of the MiSCR compiler, try the
playground! It will read input from stdin and pretty-print internal data
structures such as token vectors and abstract syntax trees.

## MiSCR Language Reference

A MiSCR file (ending in `.miscr`) contains a list of declarations. A
declaration is a module, a `data` type, or a function.

### Module System

A module contains a list of declarations.

    module CoolMath {
      data Fraction(num: i32, den: i32)

      func mul(r1: Fraction, r2: Fraction): Fraction =
        Fraction(r1.num * r2.num, r1.den * r2.den);
    }

Decls can be accessed via a path relative to the "current scope" (e.g.,
`CoolMath::mul`).

### References

There are two types of references: borrowed references (denoted with `&`) and
owned references (denoted with `#`).

Let's look at borrowed references first:

    // allocate new stack memory and initialize with value Person("Bob", 42)
    let bobRef: &Person = &Person("Bob", 42);

    // dereference operator
    let bob: Person = bob!;

    // pointer offset calculation
    let ageRef: &i32 = bobRef[.age];

Owned references point to heap-allocated memory that must eventually be freed.
The borrow checker tracks _ownership_ of owned references similar to Rust.

    // malloc creates an owned ref, which is moved to `name`
    let name: #i8 = C::malloc(7);

    // `name` is moved to `name2`
    let name2: #i8 = name;

    // It is now illegal to use `name`.
    // let name3: #i8 = name;

    // `name2` can be _borrowed_, which does not count as a _use_.
    let borrowedName: &i8 = borrow name2;
    C::strcpy(borrowedName, "hello\n");

    // `name2` must be used before the scope ends
    C::free(name2);

    // borrowedName can still be used after name2 is freed because there is no
    // lifetime analysis like Rust. So be careful with borrowed references!
    // C::write(0, borrowedName, 6);

Omitting the namespace or module declaration is the same as using `module global;`

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

## Access Paths and Borrow Checking

For every identifier, there is a tree of memory locations that can be reached
via struct projections and dereferencing. For example, let's say we have a
linked list type that contains C-style strings:

    abstract data List()
    data Cons(super: List, head: &i8, tail: #List)
    data Nil(super: List)

Now imagine every memory location you could access starting from a List `l`.
Here's how you would access them all in code. Depending on what the value of `l`
turns out to be, some of these will be invalid.

    l            : List
    l.head       : &i8
    l.head!      : i8
    l.tail       : #List
    l.tail!      : List
    l.tail!.head : &i8
    ...

The sequences of field accesses (i.e., projections) and dereferencing that is
used to reach a particular value (really its memory location) forms a "path"
through memory that is called the "access path".

Every access path is one of three types:
* Positive Access Path (PAP) -- owned by the current scope
* Zero Access Path (ZAP) -- owned by a surrounding scope
* Negative Access Path (NAP) -- a "missing" value

Suppose an identifier `x: &#i8` is in scope. Then, `x!` is an access path. There are four things you can do with an access path:

    myfunc(x!)            // use
    let y = borrow x!;    // borrow
    let z = move x;       // move
    x := C::malloc(10);   // set

The actions that are allowed depend on whether `x!` is a positive, zero, or negative access path:

|     | use | borrow | move | set |
|-----|-----|--------|------|-----|
| PAP | yes |  yes   | no   | no  |
| ZAP | no  |  yes   | yes  | no  |
| NAP | no  |  no    | no   | yes |

## Safety Properties:

MISCR should guarantee the following safety properties:

* Every malloc-d reference is freed exactly once

MISCR does _not_ guarantee these:

The absence of use-after-frees. There is no lifetime analysis, so borrowed references are just as unsafe as C pointers. e.g.,

```
func main(): i32 = {
  let x: #i8 = C::malloc(10);
  let y: &i8 = borrow x;
  C::free(x);
  C::write(0, y, 10);   // SEGFAULT
};
```

## Problems with Double-Owned Pointers:

A generic `free` function should look like this:

    extern func free<T>(ptr: #T): unit;

But what if `T` is itself an owned reference? The current move rule is that
every owned ref _not_ hidden behind a borrowed pointer gets moved when passed
to a function. However, `free` will _not recursively_ free the pointers.
We need some way to cast a `##something` into a `#unit` so that `free` can
properly handle it.

## Notes:

Use `clang -mllvm -opaque-pointers` to compile with clang version 14.

TODO: Make a better error for when you use a name that has no binding.
e.g.,

    data String(ptr: #i8, len: i64)
    func foo(p: #i8): String = {
      String(ptr, 10)
    };

C++ Core Documentation
https://llvm.org/doxygen/group__LLVMCCore.html

Minimal Readline replacement:
https://github.com/antirez/linenoise?tab=BSD-2-Clause-1-ov-file
