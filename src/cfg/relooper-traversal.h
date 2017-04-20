/*
 * Copyright 2017 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Convert the AST to the Relooper CFG.
//

#ifndef relooper_traversal_h
#define relooper_traversal_h

#include "wasm.h"
#include "wasm-builder.h"
#include "wasm-traversal.h"
#include "cfg/cfg-traversal.h"
#include "cfg/Relooper.h"

namespace wasm {

// XXX is it ok to do unified here?
struct RelooperWalker : public BasicCFGWalker<RelooperWalker, UnifiedExpressionVisitor<RelooperWalker>, CFG::Block*> {
  std::unique_ptr<Relooper> relooper;

  CFG::Block* makeBasicBlock() {
    // create a block with an empty wasm block inside, ready to receive code
    auto* ret = new Block(Builder(*getModule()).makeBlock());
    relooper->AddBlock(ret);
    return ret;
  }

  void link(CFG::Block* from, CFG::Block* to, Expression* condition) {
    if (!from || !to) return; // if one of them is not reachable, ignore
    // XXX check for only having one with nullptr condition
    from->AddBranchTo(to, condition);
  }

  void visitExpression(Expression* curr) {
    // If the current node has the none type, we add it to the current block. Otherwise,
    // it must be held
    XXX
  }

  void doWalkFunction(Function* func) {
    relooper = make_unique<Relooper>();
    BasicCFGWalker<RelooperWalker, UnifiedExpressionVisitor<RelooperWalker>, CFG::Block*>::doWalkFunction(func);
  }
};

} // namespace wasm

#endif // relooper_traversal_h
