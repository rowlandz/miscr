#include "borrowchecker/AccessPath.hpp"
#include "test.hpp"

namespace AccessPathTests {
  TESTGROUP("Access Path Tests")

  //==========================================================================//

  TEST(root_uniquing) {
    AccessPathManager apm;
    auto bob1 = apm.getRoot("bob");
    auto joe1 = apm.getRoot("joe");
    auto bob2 = apm.getRoot("bob");
    auto joe2 = apm.getRoot("joe");
    ASSERT(bob1 == bob2, "");
    ASSERT(joe1 == joe2, "");
    ASSERT(bob1 != joe1, "");
    SUCCESS
  }

  TEST(more_complex_uniquing) {
    AccessPathManager apm;
    auto path1 = apm.getProject(apm.getRoot("bob"), "name", false);
    auto path2 = apm.getProject(apm.getRoot("bob"), "name", true);
    auto path3 = apm.getProject(apm.getRoot("bob"), "name", false);
    auto path4 = apm.getProject(apm.getRoot("joe"), "name", false);
    auto path5 = apm.getProject(apm.getRoot("bob"), "age", false);
    auto path6 = apm.getDeref(apm.getProject(apm.getRoot("bob"), "age", false));
    ASSERT(path1 != path2, "");
    ASSERT(path1 == path3, "");
    ASSERT(path1 != path4, "");
    ASSERT(path1 != path5, "");
    ASSERT(path5 != path6, "");
    SUCCESS
  }

  // B[.f]! == B!.f
  TEST(addrcalc_deref_transformation) {
    AccessPathManager apm;
    auto path1 = apm.getDeref(apm.getProject(apm.getRoot("B"), "f", true));
    auto path2 = apm.getProject(apm.getDeref(apm.getRoot("B")), "f", false);
    ASSERT(path1 == path2, "B[.f]! should equal B!.f");
    SUCCESS
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
    ASSERT(path1 == path2, "B[.f1][.f2][.f3]! should equal B!.f1.f2.f3");
    SUCCESS
  }

  TEST(find_methods) {
    AccessPathManager apm;
    auto path1 = apm.getProject(apm.getRoot("bob"), "name", false);
    ASSERT(apm.findRoot("bob") != nullptr, "");
    ASSERT(apm.findProject(apm.getRoot("bob"), "name", false) == path1, "");
    ASSERT(apm.findProject(apm.getRoot("bob"), "age", false) == nullptr, "");
    SUCCESS
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
    ASSERT(bobnamederef ==
      apm.getDeref(apm.getProject(apm.getRoot("bob"), "name", false)), "");
    ASSERT(bobnamederef->asString() == "bobnamederef", "");
    SUCCESS
  }

}