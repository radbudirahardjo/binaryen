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

#ifndef cfg_traversal_h
#define cfg_traversal_h

#include "wasm.h"
#include "wasm-traversal.h"
#include "cfg/cfg-traversal.h"
#include "cfg/Relooper.h"

namespace wasm {

struct RelooperWalker : public CFGWalker<RelooperWalker, UnifiedExpressionVisitor<RelooperWalker>, CFG::Block> {

  void visitExpression(Expression* curr) {
    // If the current node has the none type, we add it to the current block. Otherwise,
    // it must be held
  }
};

} // namespace wasm

#endif // cfg_traversal_h
