#!/usr/bin/bash

HELPSTRING="Hello!

Usage:
  $0             display this help message
  $0 clean       delete previously built files
  $0 compiler    build the compiler
  $0 playground  build the playground
  $0 tests       build unit tests
"

BLUE="\x1B[34m"
YELLOW="\x1B[33m"
NOCOLOR="\x1B[0m"

DIR=$(dirname $0)

# Prefix a command with parrot to print out the command before running it
parrot () {
  echo -e "$BLUE+ $@$NOCOLOR"
  $@
}

printExtraArgumentsMessage () {
  echo -en "${YELLOW}I'll $1, but you added extra arguments that I "
  echo -e "don't understand.\nI don't understand starting from $2.$NOCOLOR"
}

################################################################################

if [ $# = 0 ]; then
  echo -n "$HELPSTRING"


elif [ $1 = "clean" ]; then
  if [ $# -gt 1 ]; then
    printExtraArgumentsMessage clean $2
  fi
  if [ -f $DIR/compiler ]; then parrot rm $DIR/compiler; fi
  if [ -f $DIR/playground ]; then parrot rm $DIR/playground; fi
  if [ -f $DIR/tests ]; then parrot rm $DIR/tests; fi
  if [ -f $DIR/src/test/testmain.cpp ]; then parrot rm $DIR/src/test/testmain.cpp; fi


elif [ $1 = "compiler" ]; then
  if [ $# -gt 1 ]; then
    printExtraArgumentsMessage compile $2
  fi
  LLVM_CONFIG_ARGS=$(llvm-config-14 --cxxflags --ldflags --system-libs --libs core)
  parrot clang++ -o $DIR/compiler $DIR/src/main/main.cpp -I$DIR/src/main $LLVM_CONFIG_ARGS


elif [ $1 = "playground" ]; then
  if [ $# -gt 1 ]; then
    printExtraArgumentsMessage "build the playground" $2
  fi
  parrot clang++ -o $DIR/playground $DIR/src/play/main.cpp -I$DIR/src/main


elif [ $1 = "tests" ]; then
  if [ $# -gt 1 ]; then
    printExtraArgumentsMessage "build tests" $2
  fi
  if [ -f $DIR/src/test/testmain.cpp ]; then
    echo -e "$YELLOW! $DIR/src/test/testmain.cpp already exists, so I'll overwrite it.$NOCOLOR"
    rm $DIR/src/test/testmain.cpp
  fi
  echo -e "$BLUE~ Making $DIR/src/test/testmain.cpp$NOCOLOR"
  TESTFILES=($DIR/src/test/*.cpp)
  for TESTFILE in ${TESTFILES[@]}; do
    BASENAME=$(basename $TESTFILE)
    echo "#include <$BASENAME>" >> $DIR/src/test/testmain.cpp
  done
  echo -en "\nint main() {\n  return 0" >> $DIR/src/test/testmain.cpp
  for TESTFILE in ${TESTFILES[@]}; do
    BASENAME=$(basename $TESTFILE)
    GROUPNAME=${BASENAME/.cpp/}
    echo -en "\n  | $GROUPNAME::testGroup.runAll()" >> $DIR/src/test/testmain.cpp
  done
  echo -e ";\n}" >> $DIR/src/test/testmain.cpp
  parrot clang++ -o $DIR/tests $DIR/src/test/testmain.cpp -I$DIR/src/test -I$DIR/src/main


else
  echo "Sorry, I don't understand those arguments."
  echo "You can run this script with no arguments to get a help message."
fi