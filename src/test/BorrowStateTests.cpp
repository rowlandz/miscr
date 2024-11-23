#include "borrowchecker/BorrowState.hpp"
#include "test.hpp"

namespace BorrowStateTests {
  TESTGROUP("Borrow State Tests")

  TEST(using_paths) {
    AccessPathManager apm;
    llvm::SmallVector<LocatedError> errors;
    BorrowState bs(errors);
    Location l(1, 1, 0);

    bs.intro(apm.getRoot("x"), l);
    bs.intro(apm.getRoot("y"), l);
    ASSERT(bs.use(apm.getRoot("x"), l), "use failed");
    ASSERT(!bs.use(apm.getRoot("x"), l), "use should have failed");
    ASSERT(bs.use(apm.getRoot("y"), l), "use failed 2");
    SUCCESS
  }

  TEST(moving_paths) {
    AccessPathManager apm;
    llvm::SmallVector<LocatedError> errors;
    BorrowState bs(errors);
    Location l(1, 1, 0);

    ASSERT(bs.move(apm.getRoot("x"), l), "move failed");
    ASSERT(!bs.move(apm.getRoot("x"), l), "move should have failed");
    ASSERT(bs.unmove(apm.getRoot("x"), l), "unmove failed");
    ASSERT(!bs.unmove(apm.getRoot("x"), l), "unmove should have failed");
    ASSERT(bs.move(apm.getRoot("x"), l), "move failed 2");
    SUCCESS
  }
}