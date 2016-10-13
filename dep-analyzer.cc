// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/Stmt.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnostic.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/CommandLine.h"
#include <iostream>

// Using clang without importing namespaces is damn near impossible.
using namespace llvm; // NOLINT
using namespace clang::ast_matchers; // NOLINT

using llvm::opt::OptTable;
using namespace clang;
using clang::ast_type_traits::DynTypedNode;
using clang::driver::createDriverOptTable;
using clang::driver::options::OPT_ast_dump;
using clang::tooling::CommonOptionsParser;
using clang::tooling::CommandLineArguments;
using clang::tooling::ClangTool;
using clang::tooling::newFrontendActionFactory;
using namespace std;

static llvm::cl::OptionCategory gToolCategory("my tool options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// Command line flags.
static cl::opt<bool> gASTDump("ast-dump",
                             cl::desc("dump ast of matched nodes"),
                             cl::cat(gToolCategory));
static llvm::cl::opt<string> gClassOpt("class",
                                       llvm::cl::desc("the class to analyze"),
                                       llvm::cl::value_desc("class"),
                                       cl::cat(gToolCategory));


namespace {

// Callback for unused statuses. Simply reports the error at the point where the
// expression was found.
class Callback : public MatchFinder::MatchCallback {
 public:
  Callback() {
  }

  virtual void run(const MatchFinder::MatchResult& result) {
    auto memberNode = result.Nodes.getNodeAs<MemberExpr>("member");
    auto functionNode = result.Nodes.getNodeAs<FunctionDecl>("containing-function");

    if (!member || !functionNode) {
      cout << "bad match!" << endl;
      return;
    }

    string member = memberNode->getMemberNameInfo().getAsString();
    string function = functionNode->getNameInfo().getAsString();
    deps_.emplace(GetOrMakeNodeId(function),
                  GetOrMakeNodeId(member));
  }

  void DumpDot() {
    cout << "digraph deps {" << endl;
    for (const auto& e : node_ids_) {
      cout << "N" << e.second << " [label=\"" << e.first << "\"];" << endl;
    }
    for (const auto& p : deps_) {
      cout << "N" << p.first << " -> N" << p.second << ";" << endl;
    }
    cout << "}\n";
  }

 private:
  int GetOrMakeNodeId(const string& name) {
    int& id = node_ids_[name];
    if (id == 0) {
      id = ++id_gen_;
    }
    return id;
  }

  set<pair<int, int>> deps_;
  map<string, int> node_ids_;
  int id_gen_ = 0;
};

} // anonymous namespace

int main(int argc, const char **argv) {
  CommonOptionsParser options_parser(argc, argv, gToolCategory);
  ClangTool Tool(options_parser.getCompilations(),
                 options_parser.getSourcePathList());

  auto pattern = gClassOpt.getValue();
  /*  auto member_reference_matcher =
      functionDecl(hasDescendant(memberExpr(hasObjectExpression(cxxThisExpr()))
                                 .bind("member")),
                   hasDeclContext(cxxRecordDecl(matchesName(pattern))))
      .bind("containing-function");
  */
  auto member_reference_matcher =
      memberExpr(hasObjectExpression(cxxThisExpr()),
                 hasAncestor(functionDecl(hasDeclContext(cxxRecordDecl(matchesName(pattern))))
                             .bind("containing-function")))
      .bind("member");
  Callback ref_member_printer;

  MatchFinder finder;
  finder.addMatcher(member_reference_matcher, &ref_member_printer);

  int ret = Tool.run(newFrontendActionFactory(&finder).get());
  ref_member_printer.DumpDot();
  return ret;

}
