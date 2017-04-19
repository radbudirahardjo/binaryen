/*
 * Copyright 2016 WebAssembly Community Group participants
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
// Convert the AST to a CFG, while traversing it.
//
// Note that this is not the same as the relooper CFG. The relooper is
// designed for compilation to an AST, this is for processing. There is
// no built-in support for transforming this CFG into the AST back
// again, it is just metadata on the side for computation purposes.
// If you do want to convert back, see relooper-traversal.h.
//
// Usage: As the traversal proceeds, you can note information and add it to
// the current basic block using getCurrBasicBlock(), on the contents
// property, whose type is user-defined.
//

#ifndef cfg_traversal_h
#define cfg_traversal_h

#include "wasm.h"
#include "wasm-traversal.h"

namespace wasm {

// A basic CFG walkers that has hooks for generating a CFG, but leaves
// the CFG data structures to be implemented by inheritors.
template<typename SubType, typename VisitorType, typename BasicBlockPtr>
struct BasicCFGWalker : public ControlFlowWalker<SubType, VisitorType> {
  SubType* self() {
    return static_cast<SubType*>(this);
  }

  // the current block in play during traversal. can be nullptr if unreachable,
  // but note that we don't do a deep unreachability analysis - just enough
  // to avoid constructing obviously-unreachable blocks (we can do a full reachability
  // analysis on the CFG once it is constructed).
  BasicBlockPtr getCurrBasicBlock() {
    return currBasicBlock;
  }

  BasicBlockPtr getEntryBlock() {
    return entryBlock;
  }

  // hook to create a new basic block
  BasicBlockPtr makeBasicBlock() {
    WASM_UNREACHABLE();
  }

  // start a new basic block
  void startBasicBlock() {
    currBasicBlock = self()->makeBasicBlock();
  }

  // start a new basic block that is unreachable
  void startUnreachableBlock() {
    currBasicBlock = nullptr;
  }

  // hook to link two basic blocks
  //  @condition the condition on which the branch depends (e.g., the if condition
  //             in an if-true branch, or nullptr if the default branch (i.e., taken
  //             if no other branch is taken)
  void link(BasicBlockPtr from, BasicBlockPtr to, Expression* condition) {
    WASM_UNREACHABLE();
  }

  // impl

  struct BlockCondition {
    BasicBlockPtr block;
    Expression* condition; // may be null, see link()
    BlockCondition(BasicBlockPtr block, Expression* condition) : block(block), condition(condition) {}
  };

  std::map<Expression*, std::vector<BlockCondition>> branches; // a block or loop => its branches
  std::vector<BasicBlockPtr> ifStack;
  std::vector<BasicBlockPtr> loopStack;

  static void doStartUnreachableBlock(SubType* self, Expression** currp) {
    self->startUnreachableBlock();
  }

  static void doEndBlock(SubType* self, Expression** currp) {
    auto* curr = (*currp)->cast<Block>();
    if (!curr->name.is()) return;
    auto iter = self->branches.find(curr);
    if (iter == self->branches.end()) return;
    auto& origins = iter->second;
    if (origins.size() == 0) return;
    // we have branches to here, so we need a new block
    auto* last = self->currBasicBlock;
    self->startBasicBlock();
    self->link(last, self->currBasicBlock, nullptr); // fallthrough
    // branches to the new one
    for (auto& origin : origins) {
      self->link(origin.block, self->currBasicBlock, origin.condition);
    }
    self->branches.erase(curr);
  }

  static void doStartIfTrue(SubType* self, Expression** currp) {
    auto* last = self->currBasicBlock;
    self->startBasicBlock();
    self->link(last, self->currBasicBlock, (*currp)->cast<If>()->condition); // ifTrue
    self->ifStack.push_back(last); // the block before the ifTrue
  }

  static void doStartIfFalse(SubType* self, Expression** currp) {
    self->ifStack.push_back(self->currBasicBlock); // the ifTrue fallthrough
    self->startBasicBlock();
    self->link(self->ifStack[self->ifStack.size() - 2], self->currBasicBlock, nullptr); // before if -> ifFalse
  }

  static void doEndIf(SubType* self, Expression** currp) {
    auto* last = self->currBasicBlock;
    self->startBasicBlock();
    self->link(last, self->currBasicBlock, nullptr); // last one is ifFalse's fallthrough if there was one, otherwise it's the ifTrue fallthrough
    if ((*currp)->cast<If>()->ifFalse) {
      // we just linked ifFalse, need to link ifTrue to the end
      self->link(self->ifStack.back(), self->currBasicBlock, nullptr);
      self->ifStack.pop_back();
    } else {
      // no ifFalse, so add a fallthrough for if the if is not taken
      self->link(self->ifStack.back(), self->currBasicBlock, nullptr);
    }
    self->ifStack.pop_back();
  }

  static void doStartLoop(SubType* self, Expression** currp) {
    auto* last = self->currBasicBlock;
    self->startBasicBlock();
    self->loopTops.push_back(self->currBasicBlock); // a loop with no backedges would still be counted here, but oh well
    self->link(last, self->currBasicBlock, nullptr);
    self->loopStack.push_back(self->currBasicBlock);
  }

  static void doEndLoop(SubType* self, Expression** currp) {
    auto* last = self->currBasicBlock;
    self->startBasicBlock();
    self->link(last, self->currBasicBlock, nullptr); // fallthrough
    auto* curr = (*currp)->cast<Loop>();
    // branches to the top of the loop
    if (curr->name.is()) {
      auto* loopStart = self->loopStack.back();
      auto& origins = self->branches[curr];
      for (auto& origin : origins) {
        self->link(origin.block, loopStart, origin.condition);
      }
      self->branches.erase(curr);
    }
    self->loopStack.pop_back();
  }

  static void doEndBreak(SubType* self, Expression** currp) {
    auto* curr = (*currp)->cast<Break>();
    self->branches[self->findBreakTarget(curr->name)].emplace_back(self->currBasicBlock, curr->condition); // branch to the target
    if (curr->condition) {
      auto* last = self->currBasicBlock;
      self->startBasicBlock();
      self->link(last, self->currBasicBlock, nullptr); // we might fall through
    } else {
      self->startUnreachableBlock();
    }
  }

  static void doEndSwitch(SubType* self, Expression** currp) {
    auto* curr = (*currp)->cast<Switch>();
    std::set<Name> seen; // we might see the same label more than once; do not spam branches
    for (Name target : curr->targets) {
      if (!seen.count(target)) {
        self->branches[self->findBreakTarget(target)].emplace_back(self->currBasicBlock, nullptr /* XXX */); // branch to the target
        seen.insert(target);
      }
    }
    if (!seen.count(curr->default_)) {
      self->branches[self->findBreakTarget(curr->default_)].emplace_back(self->currBasicBlock, nullptr);
    }
    self->startUnreachableBlock();
  }

  static void scan(SubType* self, Expression** currp) {
    Expression* curr = *currp;

    switch (curr->_id) {
      case Expression::Id::BlockId: {
        self->pushTask(SubType::doEndBlock, currp);
        break;
      }
      case Expression::Id::IfId: {
        self->pushTask(SubType::doEndIf, currp);
        auto* ifFalse = curr->cast<If>()->ifFalse;
        if (ifFalse) {
          self->pushTask(SubType::scan, &curr->cast<If>()->ifFalse);
          self->pushTask(SubType::doStartIfFalse, currp);
        }
        self->pushTask(SubType::scan, &curr->cast<If>()->ifTrue);
        self->pushTask(SubType::doStartIfTrue, currp);
        self->pushTask(SubType::scan, &curr->cast<If>()->condition);
        return; // don't do anything else
      }
      case Expression::Id::LoopId: {
        self->pushTask(SubType::doEndLoop, currp);
        break;
      }
      case Expression::Id::BreakId: {
        self->pushTask(SubType::doEndBreak, currp);
        break;
      }
      case Expression::Id::SwitchId: {
        self->pushTask(SubType::doEndSwitch, currp);
        break;
      }
      case Expression::Id::ReturnId: {
        self->pushTask(SubType::doStartUnreachableBlock, currp);
        break;
      }
      case Expression::Id::UnreachableId: {
        self->pushTask(SubType::doStartUnreachableBlock, currp);
        break;
      }
      default: {}
    }

    ControlFlowWalker<SubType, VisitorType>::scan(self, currp);

    switch (curr->_id) {
      case Expression::Id::LoopId: {
        self->pushTask(SubType::doStartLoop, currp);
        break;
      }
      default: {}
    }
  }


  void doWalkFunction(Function* func) {
    self()->startBasicBlock();
    entryBlock = getCurrBasicBlock();
    ControlFlowWalker<SubType, VisitorType>::doWalkFunction(func);
    assert(branches.size() == 0);
    assert(ifStack.size() == 0);
    assert(loopStack.size() == 0);
  }

private:
  BasicBlockPtr currBasicBlock,
                entryBlock;
};

// Very simple basic block, having only vectors of branches
// in and out, plus a custom element for contents
template<typename Contents>
struct SimpleBasicBlock {
  Contents contents;
  std::vector<SimpleBasicBlock<Contents>*> out, in;
};

// Generates a simple CFG, where the blocks are SimpleBasicBlocks.
template<typename SubType, typename VisitorType, typename Contents>
struct CFGWalker : public BasicCFGWalker<SubType, VisitorType, SimpleBasicBlock<Contents>*> {
  SubType* self() {
    return static_cast<SubType*>(this);
  }

  // public interface

  typedef SimpleBasicBlock<Contents> BasicBlock;

  BasicBlock* makeBasicBlock() { // override this with code to create a BasicBlock if necessary
    return new BasicBlock();
  }

  // internal details

  std::vector<std::unique_ptr<BasicBlock>> basicBlocks; // all the blocks
  std::vector<BasicBlock*> loopTops; // blocks that are the tops of loops, i.e., have backedges to them

  // traversal state

  void startBasicBlock() {
    BasicCFGWalker<SubType, VisitorType, BasicBlock*>::startBasicBlock();
    basicBlocks.push_back(std::unique_ptr<BasicBlock>(self()->getCurrBasicBlock()));
  }

  void link(BasicBlock* from, BasicBlock* to, Expression* condition) {
    if (!from || !to) return; // if one of them is not reachable, ignore
    from->out.push_back(to);
    to->in.push_back(from);
  }

  void doWalkFunction(Function* func) {
    basicBlocks.clear();
    BasicCFGWalker<SubType, VisitorType, BasicBlock*>::doWalkFunction(func);
  }

  std::unordered_set<BasicBlock*> findLiveBlocks() {
    std::unordered_set<BasicBlock*> alive;
    std::unordered_set<BasicBlock*> queue;
    queue.insert(self()->getEntryBlock());
    while (queue.size() > 0) {
      auto iter = queue.begin();
      auto* curr = *iter;
      queue.erase(iter);
      alive.insert(curr);
      for (auto* out : curr->out) {
        if (!alive.count(out)) queue.insert(out);
      }
    }
    return alive;
  }

  void unlinkDeadBlocks(std::unordered_set<BasicBlock*> alive) {
    for (auto& block : basicBlocks) {
      if (!alive.count(block.get())) {
        block->in.clear();
        block->out.clear();
        continue;
      }
      block->in.erase(std::remove_if(block->in.begin(), block->in.end(), [&alive](BasicBlock* other) {
        return !alive.count(other);
      }), block->in.end());
      block->out.erase(std::remove_if(block->out.begin(), block->out.end(), [&alive](BasicBlock* other) {
        return !alive.count(other);
      }), block->out.end());
    }
  }

  // TODO: utility method for optimizing cfg, removing empty blocks depending on their .content

  std::map<BasicBlock*, size_t> debugIds;

  void generateDebugIds() {
    if (debugIds.size() > 0) return;
    for (auto& block : basicBlocks) {
      debugIds[block.get()] = debugIds.size();
    }
  }

  void dumpCFG(std::string message) {
    std::cout << "<==\nCFG [" << message << "]:\n";
    generateDebugIds();
    for (auto& block : basicBlocks) {
      assert(debugIds.count(block.get()) > 0);
      std::cout << "  block " << debugIds[block.get()] << ":\n";
      block->contents.dump(static_cast<SubType*>(this)->getFunction());
      for (auto& in : block->in) {
        assert(debugIds.count(in) > 0);
        assert(std::find(in->out.begin(), in->out.end(), block.get()) != in->out.end()); // must be a parallel link back
      }
      for (auto& out : block->out) {
        assert(debugIds.count(out) > 0);
        std::cout << "    out: " << debugIds[out] << "\n";
        assert(std::find(out->in.begin(), out->in.end(), block.get()) != out->in.end()); // must be a parallel link back
      }
      checkDuplicates(block->in);
      checkDuplicates(block->out);
    }
    std::cout << "==>\n";
  }

private:
  // links in out and in must be unique
  void checkDuplicates(std::vector<BasicBlock*>& list) {
    std::unordered_set<BasicBlock*> seen;
    for (auto* curr : list) {
      assert(seen.count(curr) == 0);
      seen.insert(curr);
    }
  }

  void removeLink(std::vector<BasicBlock*>& list, BasicBlock* toRemove) {
    if (list.size() == 1) {
      list.clear();
      return;
    }
    for (size_t i = 0; i < list.size(); i++) {
      if (list[i] == toRemove) {
        list[i] = list.back();
        list.pop_back();
        return;
      }
    }
    WASM_UNREACHABLE();
  }
};

} // namespace wasm

#endif // cfg_traversal_h
