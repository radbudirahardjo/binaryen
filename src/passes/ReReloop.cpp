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
// Convert the AST to a CFG, and optimize+convert it back to the AST
// using the relooper
//

#include "wasm.h"
#include "wasm-builder.h"
#include "wasm-traversal.h"
#include "cfg/Relooper.h"

namespace wasm {

struct ReRelooper : public Visitor<ReRelooper, void> {
  RelooperBuilder builder;
  Relooper relooper;

  CFG::Block* currBlock;

  ReRelooper(Module& wasm, Function* func) : builder(wasm) {
    // create the entry block, and start to process the function body
    makeCurrBlock();
    auto* entry = currBlock;
    visit(func->body);
    // reloop it
    relooper.Calculate(entry);
    func->body = relooper.Render(builder);
  }

  void makeCurrBlock();
    // create a block with an empty wasm block inside, ready to receive code
    currBlock = new Block(Builder(*getModule()).makeBlock());
    relooper->AddBlock(currBlock);
  }

  void visitBlock(Block* curr) {}
  void visitIf(If* curr) {}
  void visitLoop(Loop* curr) {}
  void visitBreak(Break* curr) {}
  void visitSwitch(Switch* curr) {}
  void visitCall(Call* curr) {}
  void visitCallImport(CallImport* curr) {}
  void visitCallIndirect(CallIndirect* curr) {}
  void visitGetLocal(GetLocal* curr) {}
  void visitSetLocal(SetLocal* curr) {}
  void visitGetGlobal(GetGlobal* curr) {}
  void visitSetGlobal(SetGlobal* curr) {}
  void visitLoad(Load* curr) {}
  void visitStore(Store* curr) {}
  void visitConst(Const* curr) {}
  void visitUnary(Unary* curr) {}
  void visitBinary(Binary* curr) {}
  void visitSelect(Select* curr) {}
  void visitDrop(Drop* curr) {}
  void visitReturn(Return* curr) {}
  void visitHost(Host* curr) {}
  void visitNop(Nop* curr) {}
  void visitUnreachable(Unreachable* curr) {}
};

struct ReReloopPass : public Pass {
  // TODO: parallelize
  void run(PassRunner* runner, Module* module) override {
    for (auto& func : module->functions) {
      ReRelooper reRelooper(*module, func.get());
    }
  }
}

Pass *createReReloopPass() {
  return new ReReloopPass();
}

} // namespace wasm

