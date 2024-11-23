#ifndef TEST_HPP
#define TEST_HPP

#include <cstdio>
#include <functional>
#include <optional>
#include <vector>

#define TESTGROUP(name) UnitTestGroup testGroup(name);
#define TEST(name) std::optional<std::string> name();\
  char _##name = ([](){ testGroup.tests.push_back(UnitTest(#name, name));\
  return 0; })(); std::optional<std::string> name()

/// Run the expression. Return the result if it is convertable to `true`.
#define TRY(exp) { if (auto error = exp) return error; }

/// Return with `errmsg` if `condition` is false.
#define ASSERT(condition, errmsg) { if (!(condition)) return errmsg; }

/// Successfully return from a test.
#define SUCCESS { return std::optional<std::string>(); }

class UnitTest {
  const char* name;
  std::function<std::optional<std::string>()> code;

public:
  UnitTest(const char* name, std::function<std::optional<std::string>()> code)
    : name(name), code(code) {}

  int run() {
    if (auto error = code()) {
      printf("\x1B[31m - %s\n%s\x1B[0m\n", name, error.value().c_str());
      return 1;
    } else {
      printf("\x1B[32m - %s\x1B[0m\n", name);
      return 0;
    }
  }
};

struct UnitTestGroup {
  const char* name;
  std::vector<UnitTest> tests;

  UnitTestGroup(const char* name) : name(name) {}

  int runAll() {
    int exitcode = 0;
    printf("%s\n", name);
    for (auto t: tests) { exitcode |= t.run(); }
    return exitcode;
  }
};

#endif