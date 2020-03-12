#include "cfg/CFG.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_split.h"
#include "common/Timer.h"
#include "common/formatting.h"

// helps debugging
template class std::unique_ptr<sorbet::cfg::CFG>;
template class std::unique_ptr<sorbet::cfg::BasicBlock>;
template class std::vector<sorbet::cfg::BasicBlock *>;

using namespace std;

namespace sorbet::cfg {

BasicBlock *CFG::freshBlock(int outerLoops, int rubyBlockId) {
    int id = this->maxBasicBlockId++;
    auto &r = this->basicBlocks.emplace_back(make_unique<BasicBlock>());
    r->id = id;
    r->outerLoops = outerLoops;
    r->rubyBlockId = rubyBlockId;
    return r.get();
}

CFG::CFG() {
    freshBlock(0, 0); // entry;
    freshBlock(0, 0); // dead code;
    deadBlock()->bexit.elseb = deadBlock();
    deadBlock()->bexit.thenb = deadBlock();
    deadBlock()->bexit.cond.variable = core::LocalVariable::noVariable();
}

CFG::ReadsAndWrites CFG::findAllReadsAndWrites(core::Context ctx) {
    Timer timeit(ctx.state.tracer(), "findAllReadsAndWrites");
    CFG::ReadsAndWrites target;
    target.reads.resize(maxBasicBlockId);
    target.writes.resize(maxBasicBlockId);
    target.dead.resize(maxBasicBlockId);
    vector<vector<bool>> readsAndWrites(maxBasicBlockId);

    for (unique_ptr<BasicBlock> &bb : this->basicBlocks) {
        auto &blockWrites = target.writes[bb->id];
        blockWrites.resize(maxVariableId);
        auto &blockReads = target.reads[bb->id];
        blockReads.resize(maxVariableId);
        auto &blockDead = target.dead[bb->id];
        blockDead.resize(maxVariableId);
        auto &blockReadsAndWrites = readsAndWrites[bb->id];
        blockReadsAndWrites.resize(maxVariableId);
        for (Binding &bind : bb->exprs) {
            blockWrites[bind.bind.variable.id()] = true;
            blockReadsAndWrites[bind.bind.variable.id()] = true;
            /*
             * When we write to an alias, we rely on the type information being
             * propagated through block arguments from the point of
             * assignment. Treating every write as also reading from the
             * variable serves to represent this.
             */
            if (bind.bind.variable.isAliasForGlobal(*this, ctx) && cast_instruction<Alias>(bind.value.get()) == nullptr) {
                blockReads[bind.bind.variable.id()] = true;
            }

            if (auto *v = cast_instruction<Ident>(bind.value.get())) {
                blockReads[v->what.id()] = true;
                blockReadsAndWrites[v->what.id()] = true;
            } else if (auto *v = cast_instruction<Send>(bind.value.get())) {
                blockReads[v->recv.variable.id()] = true;
                blockReadsAndWrites[v->recv.variable.id()] = true;
                for (auto &arg : v->args) {
                    blockReads[arg.variable.id()] = true;
                    blockReadsAndWrites[arg.variable.id()] = true;
                }
            } else if (auto *v = cast_instruction<TAbsurd>(bind.value.get())) {
                blockReads[v->what.variable.id()] = true;
                blockReadsAndWrites[v->what.variable.id()] = true;
            } else if (auto *v = cast_instruction<Return>(bind.value.get())) {
                blockReads[v->what.variable.id()] = true;
                blockReadsAndWrites[v->what.variable.id()] = true;
            } else if (auto *v = cast_instruction<BlockReturn>(bind.value.get())) {
                blockReads[v->what.variable.id()] = true;
                blockReadsAndWrites[v->what.variable.id()] = true;
            } else if (auto *v = cast_instruction<Cast>(bind.value.get())) {
                blockReads[v->value.variable.id()] = true;
                blockReadsAndWrites[v->value.variable.id()] = true;
            } else if (auto *v = cast_instruction<LoadSelf>(bind.value.get())) {
                blockReads[v->fallback.id()] = true;
                blockReadsAndWrites[v->fallback.id()] = true;
            } else if (auto *v = cast_instruction<SolveConstraint>(bind.value.get())) {
                blockReads[v->send.id()] = true;
                blockReadsAndWrites[v->send.id()] = true;
            }

            auto fnd = blockReads[bind.bind.variable.id()];
            if (!fnd) {
                blockDead[bind.bind.variable.id()] = true;
            }
        }
        if (bb->bexit.cond.variable.exists()) {
            blockReads[bb->bexit.cond.variable.id()] = true;
            blockReadsAndWrites[bb->bexit.cond.variable.id()] = true;
        }
    }
    vector<pair<int, int>> usageCounts;
    usageCounts.resize(maxVariableId);

    {
        Timer timeit(ctx.state.tracer(), "privates1");

        for (auto blockId = 0; blockId < maxBasicBlockId; blockId++) {
                auto local = 0;
            for (auto el : readsAndWrites[blockId]) {
                    if (el) {
                if (usageCounts[local].first == 0) {
                  usageCounts[local].second = blockId;
                }
                usageCounts[local].first += 1;
                    }
                local += 1;
            }
        }
    }
    {
        Timer timeit(ctx.state.tracer(), "privates2");
        int local = 0;
        for (const auto &usages : usageCounts) {
            if (usages.first == 1) {
                target.writes[usages.second][local] = false;
            }
            local += 1;
        }
    }

    return target;
}

void CFG::sanityCheck(core::Context ctx) {
    if (!debug_mode) {
        return;
    }

    for (auto &bb : this->basicBlocks) {
        ENFORCE(bb->bexit.isCondSet(), "Block exit condition left unset for block {}", bb->toString(*this, ctx));

        if (bb.get() == deadBlock()) {
            continue;
        }

        auto thenCount = absl::c_count(bb->bexit.thenb->backEdges, bb.get());
        auto elseCount = absl::c_count(bb->bexit.elseb->backEdges, bb.get());
        ENFORCE(thenCount == 1, "bb id={}; then has {} back edges", bb->id, thenCount);
        ENFORCE(elseCount == 1, "bb id={}; else has {} back edges", bb->id, elseCount);
        if (bb->bexit.thenb == bb->bexit.elseb) {
            ENFORCE(!bb->bexit.cond.variable.exists());
        } else {
            ENFORCE(bb->bexit.cond.variable.exists());
        }
    }
}

string CFG::toString(const core::GlobalState &gs) const {
    fmt::memory_buffer buf;
    string symbolName = this->symbol.data(gs)->showFullName(gs);
    fmt::format_to(buf,
                   "subgraph \"cluster_{}\" {{\n"
                   "    label = \"{}\";\n"
                   "    color = blue;\n"
                   "    \"bb{}_0\" [shape = invhouse];\n"
                   "    \"bb{}_1\" [shape = parallelogram];\n\n",
                   symbolName, symbolName, symbolName, symbolName);
    for (auto &basicBlock : this->basicBlocks) {
        auto text = basicBlock->toString(*this, gs);
        auto lines = absl::StrSplit(text, "\n");

        fmt::format_to(
            buf,
            "    \"bb{}_{}\" [\n"
            "        label = \"{}\\l\"\n"
            "    ];\n\n"
            "    \"bb{}_{}\" -> \"bb{}_{}\" [style=\"bold\"];\n",
            symbolName, basicBlock->id,
            fmt::map_join(lines.begin(), lines.end(), "\\l", [](auto line) -> string { return absl::CEscape(line); }),
            symbolName, basicBlock->id, symbolName, basicBlock->bexit.thenb->id);

        if (basicBlock->bexit.thenb != basicBlock->bexit.elseb) {
            fmt::format_to(buf, "    \"bb{}_{}\" -> \"bb{}_{}\" [style=\"tapered\"];\n\n", symbolName, basicBlock->id,
                           symbolName, basicBlock->bexit.elseb->id);
        }
    }
    fmt::format_to(buf, "}}");
    return to_string(buf);
}

string CFG::showRaw(core::Context ctx) const {
    fmt::memory_buffer buf;
    string symbolName = this->symbol.data(ctx)->showFullName(ctx);
    fmt::format_to(buf,
                   "subgraph \"cluster_{}\" {{\n"
                   "    label = \"{}\";\n"
                   "    color = blue;\n"
                   "    \"bb{}_0\" [shape = box];\n"
                   "    \"bb{}_1\" [shape = parallelogram];\n\n",
                   symbolName, symbolName, symbolName, symbolName);
    for (auto &basicBlock : this->basicBlocks) {
        auto text = basicBlock->showRaw(*this, ctx);
        auto lines = absl::StrSplit(text, "\n");

        fmt::format_to(
            buf,
            "    \"bb{}_{}\" [\n"
            "        label = \"{}\\l\"\n"
            "    ];\n\n"
            "    \"bb{}_{}\" -> \"bb{}_{}\" [style=\"bold\"];\n",
            symbolName, basicBlock->id,
            fmt::map_join(lines.begin(), lines.end(), "\\l", [](auto line) -> string { return absl::CEscape(line); }),
            symbolName, basicBlock->id, symbolName, basicBlock->bexit.thenb->id);

        if (basicBlock->bexit.thenb != basicBlock->bexit.elseb) {
            fmt::format_to(buf, "    \"bb{}_{}\" -> \"bb{}_{}\" [style=\"tapered\"];\n\n", symbolName, basicBlock->id,
                           symbolName, basicBlock->bexit.elseb->id);
        }
    }
    fmt::format_to(buf, "}}");
    return to_string(buf);
}

string BasicBlock::toString(const CFG &cfg, const core::GlobalState &gs) const {
    fmt::memory_buffer buf;
    fmt::format_to(
        buf, "block[id={}, rubyBlockId={}]({})\n", this->id, this->rubyBlockId,
        fmt::map_join(
            this->args.begin(), this->args.end(), ", ", [&](const auto &arg) -> auto { return arg.toString(cfg, gs); }));

    if (this->outerLoops > 0) {
        fmt::format_to(buf, "outerLoops: {}\n", this->outerLoops);
    }
    for (const Binding &exp : this->exprs) {
        fmt::format_to(buf, "{} = {}\n", exp.bind.toString(cfg, gs), exp.value->toString(cfg, gs));
    }
    if (this->bexit.cond.variable.exists()) {
        fmt::format_to(buf, "{}", this->bexit.cond.toString(cfg, gs));
    } else {
        fmt::format_to(buf, "<unconditional>");
    }
    return to_string(buf);
}

string BasicBlock::showRaw(const CFG &cfg, const core::GlobalState &gs) const {
    fmt::memory_buffer buf;
    fmt::format_to(
        buf, "block[id={}]({})\n", this->id,
        fmt::map_join(
            this->args.begin(), this->args.end(), ", ", [&](const auto &arg) -> auto { return arg.showRaw(cfg, gs); }));

    if (this->outerLoops > 0) {
        fmt::format_to(buf, "outerLoops: {}\n", this->outerLoops);
    }
    for (const Binding &exp : this->exprs) {
        fmt::format_to(buf, "Binding {{\n&nbsp;bind = {},\n&nbsp;value = {},\n}}\n", exp.bind.showRaw(cfg, gs, 1),
                       exp.value->showRaw(cfg, gs, 1));
    }
    if (this->bexit.cond.variable.exists()) {
        fmt::format_to(buf, "{}", this->bexit.cond.showRaw(cfg, gs));
    } else {
        fmt::format_to(buf, "<unconditional>");
    }
    return to_string(buf);
}

Binding::Binding(LocalRef bind, core::Loc loc, unique_ptr<Instruction> value)
    : bind(bind), loc(loc), value(std::move(value)) {}

} // namespace sorbet::cfg
