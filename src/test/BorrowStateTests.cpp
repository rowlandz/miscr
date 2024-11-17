#include "borrowchecker/BorrowState.hpp"
#include "test.hpp"

namespace BorrowStateTests {
  TESTGROUP("Borrow State Tests")

  /// @brief Throws a `std::runtime_error` if @p condition is false.
  void assertt(bool condition, llvm::StringRef errMsg = "")
    { if (!condition) throw std::runtime_error(errMsg.data()); }

  TEST(using_paths) {
    AccessPathManager apm;
    llvm::SmallVector<LocatedError> errors;
    BorrowState bs(errors);
    Location l(1, 1, 0);

    bs.intro(apm.getRoot("x"), l);
    bs.intro(apm.getRoot("y"), l);
    assertt(bs.use(apm.getRoot("x"), l), "use failed");
    assertt(!bs.use(apm.getRoot("x"), l), "use should have failed");
    assertt(bs.use(apm.getRoot("y"), l), "use failed 2");
  }

  TEST(moving_paths) {
    AccessPathManager apm;
    llvm::SmallVector<LocatedError> errors;
    BorrowState bs(errors);
    Location l(1, 1, 0);

    assertt(bs.move(apm.getRoot("x"), l), "move failed");
    assertt(!bs.move(apm.getRoot("x"), l), "move should have failed");
    assertt(bs.unmove(apm.getRoot("x"), l), "unmove failed");
    assertt(!bs.unmove(apm.getRoot("x"), l), "unmove should have failed");
    assertt(bs.move(apm.getRoot("x"), l), "move failed 2");
  }
}