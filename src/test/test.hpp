#ifndef TEST_HPP
#define TEST_HPP

#include <cstdio>
#include <functional>
#include <vector>
#include <stdexcept>

#define TESTGROUP(name) UnitTestGroup testGroup(name);
#define TEST(name) void name();\
  char _##name = ([](){ testGroup.tests.push_back(UnitTest(#name, name)); return 0; })();\
  void name()

class UnitTest {
  const char* name;
  std::function<void()> code;

public:
  UnitTest(const char* name, std::function<void()> code) {
    this->name = name;
    this->code = code;
  }

  int run() {
    try { code(); }
    catch (const std::exception &e) {
      printf("\x1B[31m - %s\n%s\x1B[0m\n", name, e.what());
      return 1;
    } catch (...) {
      printf("\x1B[31m - %s\n   Unknown error occurred\x1B[0m\n", name);
      return 1;
    }
    printf("\x1B[32m - %s\x1B[0m\n", name);
    return 0;
  }
};

struct UnitTestGroup {
  const char* name;
  std::vector<UnitTest> tests;

  UnitTestGroup(const char* name) {
    this->name = name;
  }

  int runAll() {
    int exitcode = 0;
    printf("%s\n", name);
    for (auto t: tests) { exitcode |= t.run(); }
    return exitcode;
  }
};

#endif