
#include "borrowchecker/AccessPath.hpp"
#include "test.hpp"

namespace AccessPathTests {
  TESTGROUP("Access Path Tests")

  /// @brief Throws a `std::runtime_error` if @p condition is false.
  void assertt(bool condition, llvm::StringRef errMsg = "")
    { if (!condition) throw std::runtime_error(errMsg.data()); }

  //==========================================================================//

  TEST(root_uniquing) {
    AccessPathManager apm;
    auto bob1 = apm.getRoot("bob");
    auto joe1 = apm.getRoot("joe");
    auto bob2 = apm.getRoot("bob");
    auto joe2 = apm.getRoot("joe");
    assertt(bob1 == bob2);
    assertt(joe1 == joe2);
    assertt(bob1 != joe1);
  }

  TEST(more_complex_uniquing) {
    AccessPathManager apm;
    auto path1 = apm.getProject(apm.getRoot("bob"), "name", false);
    auto path2 = apm.getProject(apm.getRoot("bob"), "name", true);
    auto path3 = apm.getProject(apm.getRoot("bob"), "name", false);
    auto path4 = apm.getProject(apm.getRoot("joe"), "name", false);
    auto path5 = apm.getProject(apm.getRoot("bob"), "age", false);
    auto path6 = apm.getDeref(apm.getProject(apm.getRoot("bob"), "age", false));
    assertt(path1 != path2);
    assertt(path1 == path3);
    assertt(path1 != path4);
    assertt(path1 != path5);
    assertt(path5 != path6);
  }

  // B[.f]! == B!.f
  TEST(addrcalc_deref_transformation) {
    AccessPathManager apm;
    auto path1 = apm.getDeref(apm.getProject(apm.getRoot("B"), "f", true));
    auto path2 = apm.getProject(apm.getDeref(apm.getRoot("B")), "f", false);
    assertt(path1 == path2, "B[.f]! should equal B!.f");
  }

  // B[.f1][.f2][.f3]! == B!.f1.f2.f3
  TEST(addrcalc_deref_transformation_2) {
    AccessPathManager apm;
    auto path1 = apm.getDeref(
      apm.getProject(
        apm.getProject(
          apm.getProject(
            apm.getRoot("B"),
            "f1", true),
          "f2", true),
        "f3", true));
    auto path2 =
      apm.getProject(
        apm.getProject(
          apm.getProject(
            apm.getDeref(apm.getRoot("B")),
            "f1", false),
          "f2", false),
        "f3", false);
    assertt(path1 == path2, "B[.f1][.f2][.f3]! should equal B!.f1.f2.f3");
  }

  TEST(find_methods) {
    AccessPathManager apm;
    auto path1 = apm.getProject(apm.getRoot("bob"), "name", false);
    assertt(apm.findRoot("bob") != nullptr);
    assertt(apm.findProject(apm.getRoot("bob"), "name", false) == path1);
    assertt(apm.findProject(apm.getRoot("bob"), "age", false) == nullptr);
  }

  // create alias   bob.name |-> bobname
  // create alias   bobname! |-> bobnamederef
  // assert bobnamederef == bob.name!
  TEST(transitive_alias) {
    AccessPathManager apm;
    auto bobname = apm.getRoot("bobname");
    apm.aliasProject(apm.getRoot("bob"), "name", false, bobname);
    auto bobnamederef = apm.getRoot("bobnamederef");
    apm.aliasDeref(bobname, bobnamederef);
    assertt(bobnamederef ==
      apm.getDeref(apm.getProject(apm.getRoot("bob"), "name", false)));
    assertt(bobnamederef->asString() == "bobnamederef");
  }

}