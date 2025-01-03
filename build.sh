#!/usr/bin/bash

HELPSTRING="Usage:
  build.sh                           display this help message
  build.sh miscrc [CCOPTS...]        build the MiSCR compiler
  build.sh miscrc-static             build statically-linked miscrc
  build.sh playground                build the playground
  build.sh tests [TESTFILE.cpp...]   build unit tests
  build.sh clean                     delete previously built files

Options:
  CCOPTS    - Options passed directly to the C++ compiler. For example -g for
              debugging or -O1 for optimization.
  TESTFILE  - A file containing unit tests. If none are listed, then the tests
              subcommand will use all .cpp files in src/test.
"

CC="g++"

NOCOLOR="\x1B[0m"
RED="\x1B[31m"
GREEN="\x1B[32m"
YELLOW="\x1B[33m"
BLUE="\x1B[34m"

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

getLLVMConfigArgs () {
  if [ $(which llvm-config-18) ]; then
    echo -e "$BLUE~ Found llvm-config-18$NOCOLOR"
    LLVM_CONFIG_ARGS=$(llvm-config-18 --cxxflags --ldflags --libs core)
  elif [ $(which llvm-config) ]; then
    echo -e "$BLUE~ Found llvm-config$NOCOLOR"
    LLVM_CONFIG_ARGS=$(llvm-config --cxxflags --ldflags --libs core)
  else
    echo -e "${RED}Could not find llvm-config in PATH. I looked for:"
    echo -e "  llvm-config-18\n  llvm-config$NOCOLOR"
    echo -e "Try installing LLVM:\n  sudo apt install llvm-18"
    exit 1
  fi
}


################################################################################

if [ $# = 0 ]; then
  echo -n "$HELPSTRING"


########################################
### Subcommand: clean
###
elif [ $1 = "clean" ]; then
  if [ $# -gt 1 ]; then
    printExtraArgumentsMessage clean $2
  fi
  if [ -f $DIR/miscrc ]; then parrot rm $DIR/miscrc; fi
  if [ -f $DIR/miscrc-static ]; then parrot rm $DIR/miscrc-static; fi
  if [ -f $DIR/playground ]; then parrot rm $DIR/playground; fi
  if [ -f $DIR/tests ]; then parrot rm $DIR/tests; fi
  if [ -f $DIR/src/test/testmain.cpp ]; then
    parrot rm $DIR/src/test/testmain.cpp;
  fi


########################################
### Subcommand: miscrc
###
elif [ $1 = "miscrc" ]; then
  getLLVMConfigArgs
  parrot $CC -o $DIR/miscrc $DIR/src/main/main.cpp -I$DIR/src/main \
    $LLVM_CONFIG_ARGS ${@:2}


########################################
### Subcommand: miscrc-static
###

elif [ $1 = "miscrc-static" ]; then
  LLVM_CONFIG_ARGS=$(llvm-config-18 --cxxflags --ldflags --link-static --libs core)
  parrot $CC -static -o $DIR/miscrc-static $DIR/src/main/main.cpp \
    -I$DIR/src/main $LLVM_CONFIG_ARGS -ltinfo -O1


########################################
### Subcommand: playground
###
elif [ $1 = "playground" ]; then
  getLLVMConfigArgs
  if [ $# -gt 1 ]; then
    printExtraArgumentsMessage "build the playground" $2
  fi
  parrot $CC -o $DIR/playground $DIR/src/play/main.cpp -I$DIR/src/main \
    $LLVM_CONFIG_ARGS


########################################
### Subcommand: tests
###
elif [ $1 = "tests" ]; then
  getLLVMConfigArgs

  # validate test files (if given any)
  if [ $# -gt 1 ]; then
    TESTFILES=${@:2}
    for TESTFILE in ${TESTFILES[@]}; do
      if [ ! -f $TESTFILE ]; then
        echo -e "$RED! $TESTFILE does not exist$NOCOLOR"
        exit 1
      fi
      if [[ $TESTFILE != *.cpp ]]; then
        echo -e "$RED! $TESTFILE does not end in .cpp$NOCOLOR"
        exit 1
      fi
    done
  fi

  # delete existing testmain.cpp
  if [ -f $DIR/src/test/testmain.cpp ]; then
    echo -e "$YELLOW! $DIR/src/test/testmain.cpp already exists, so I'll overwrite it.$NOCOLOR"
    rm $DIR/src/test/testmain.cpp
  fi

  # grab all src/test/*.cpp files if no test files given
  if [ $# -le 1 ]; then
    TESTFILES=($DIR/src/test/*.cpp)
  fi

  # generate testmain.cpp
  echo -e "$BLUE~ Making $DIR/src/test/testmain.cpp$NOCOLOR"
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

  # compile testmain.cpp into tests
  parrot $CC -o $DIR/tests $DIR/src/test/testmain.cpp -I$DIR/src/test \
    -I$DIR/src/main $LLVM_CONFIG_ARGS


########################################
### Unrecognized subcommand
###
else
  echo "Sorry, I don't understand those arguments."
  echo "You can run this script with no arguments to get a help message."

fi
