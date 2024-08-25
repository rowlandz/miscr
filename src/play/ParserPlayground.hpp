#ifndef PARSERPLAYGROUND
#define PARSERPLAYGROUND

#include <functional>
#include <iostream>
#include "common/Node.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"

const char* NodeTyToString(NodeTy nt) {
  switch (nt) {
    case NodeTy::LIT_INT: return "LIT_INT";
    case NodeTy::LIT_DEC: return "LIT_DEC";
    case NodeTy::LIT_STRING: return "LIT_STRING";
    case NodeTy::IF: return "IF";
    case NodeTy::ADD: return "ADD";
    case NodeTy::SUB: return "SUB";
    case NodeTy::MUL: return "MUL";
    case NodeTy::DIV: return "DIV";
    case NodeTy::EQ: return "EQ";
    case NodeTy::NE: return "NE";
    case NodeTy::BLOCK: return "BLOCK";
    case NodeTy::CALL: return "CALL";
    case NodeTy::BOOL: return "BOOL";
    case NodeTy::TYPEVAR: return "TYPEVAR";
    case NodeTy::i8: return "i8";
    case NodeTy::i32: return "i32";
    case NodeTy::IDENT: return "IDENT";
    case NodeTy::EIDENT: return "EIDENT";
    case NodeTy::EXPLIST_NIL: return "EXPLIST_NIL";
    case NodeTy::EXPLIST_CONS: return "EXPLIST_CONS";
    case NodeTy::LET: return "LET";
    case NodeTy::RETURN: return "RETURN";
    case NodeTy::FUNC: return "FUNC";
    case NodeTy::PROC: return "PROC";
    case NodeTy::PARAMLIST_NIL: return "PARAMLIST_NIL";
    case NodeTy::PARAMLIST_CONS: return "PARAMLIST_CONS";
    case NodeTy::STMTLIST_NIL: return "STMTLIST_NIL";
    case NodeTy::STMTLIST_CONS: return "STMTLIST_CONS";
  }
}

void print_parse_tree(NodeManager &m, unsigned int n, std::vector<bool> &indents) {
  Node node = m.get(n);
  printf("ln%3d, col%3d, sz%3d   ", node.loc.row, node.loc.col, node.loc.sz);
  if (indents.size() > 0) {
    for (auto i = 0; i < indents.size() - 1; i++) {
      printf("%s   ", indents[i] ? "│" : " ");
    }
    printf("%s── ", *(indents.end()-1) ? "├" : "└");
  }
  printf("%s\n", NodeTyToString(node.ty));

  if (node.n1 != NN) {
    if (node.n2 != NN) {
      if (node.n3 != NN) {
        if (node.extra.nodes.n4 != NN) {              // TODO: this could cause problems
          if (node.extra.nodes.n5 != NN) {
            // five subnodes
            indents.push_back(true);
            print_parse_tree(m, node.n1, indents);
            print_parse_tree(m, node.n2, indents);
            print_parse_tree(m, node.n3, indents);
            print_parse_tree(m, node.extra.nodes.n4, indents);
            indents.pop_back(); indents.push_back(false);
            print_parse_tree(m, node.extra.nodes.n5, indents);
            indents.pop_back();
          } else {
            // four subnodes
            indents.push_back(true);
            print_parse_tree(m, node.n1, indents);
            print_parse_tree(m, node.n2, indents);
            print_parse_tree(m, node.n3, indents);
            indents.pop_back(); indents.push_back(false);
            print_parse_tree(m, node.extra.nodes.n4, indents);
            indents.pop_back();
          }
        } else {
          // three subnodes
          indents.push_back(true);
          print_parse_tree(m, node.n1, indents);
          print_parse_tree(m, node.n2, indents);
          indents.pop_back(); indents.push_back(false);
          print_parse_tree(m, node.n3, indents);
          indents.pop_back();
        }
      } else {
        // two subnodes
        indents.push_back(true);
        print_parse_tree(m, node.n1, indents);
        indents.pop_back(); indents.push_back(false);
        print_parse_tree(m, node.n2, indents);
        indents.pop_back();
      }
    } else {
      // one subnode
      indents.push_back(false);
      print_parse_tree(m, node.n1, indents);
      indents.pop_back();
    }
  }
}

int play_with_parser(char* grammarElement) {
  Lexer lexer;
  std::string line;

  unsigned int(Parser::*chosenParseFunction)();
  if (!strcmp(grammarElement, "decl")) chosenParseFunction = &Parser::funcOrProc;
  else if (!strcmp(grammarElement, "exp")) chosenParseFunction = &Parser::exp;
  else if (!strcmp(grammarElement, "stmt")) chosenParseFunction = &Parser::stmt;
  else {
    std::cout << "I don't know how to parse a " << grammarElement << std::endl;
    return 1;
  }

  while (!std::cin.eof()) {
    std::cout << "\x1B[34m>\x1B[0m " << std::flush;
    std::getline(std::cin, line);
    auto tokens = lexer.run(line.c_str());
    Parser parser(tokens);

    unsigned int parsed = (parser.*chosenParseFunction)();
    if (IS_ERROR(parsed))
      std::cout << "Parse error" << std::endl;
    else {
      std::vector<bool> indents;
      print_parse_tree(parser.m, parsed, indents);
    }
  }

  return 0;
}

#endif