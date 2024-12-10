#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/DebugInfo.h"
#include <set>
#include <unordered_map>
#include <string>
#include <queue>

using namespace llvm;

class KeyPointsPass : public PassInfoMixin<KeyPointsPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        errs() << "Analyzing : " << F.getName() << "\n";
        LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
        std::set<Value*> inputVariables;
        std::unordered_map<Value*, std::set<Value*>> defUseChain;

        // First pass: identify input variables and build def-use chain
        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *callInst = dyn_cast<CallInst>(&I)) {
                    if (isInputFunction(callInst)) {
                        for (Use &U : callInst->args()) {
                            if (Value *arg = U.get()) {
                                inputVariables.insert(arg);
                            }
                        }
                    }
                }
                buildDefUseChain(I, defUseChain);
            }
        }

        // Analyze loops and their termination conditions
        for (Loop *L : LI) {
            analyzeLoop(L, inputVariables, defUseChain, F);
        }

        return PreservedAnalyses::all();
    }

private:
    bool isInputFunction(CallInst *CI) {
        if (Function *F = CI->getCalledFunction()) {
            StringRef Name = F->getName();
            return Name == "scanf" || Name == "fopen" || Name == "getc";
        }
        return false;
    }

    void buildDefUseChain(Instruction &I, std::unordered_map<Value*, std::set<Value*>> &defUseChain) {
        for (Use &U : I.operands()) {
            Value *V = U.get();
            defUseChain[V].insert(&I);
        }
    }

    void analyzeLoop(Loop *L, const std::set<Value*> &inputVariables, 
                     const std::unordered_map<Value*, std::set<Value*>> &defUseChain,
                     Function &F) {
        if (BasicBlock *Header = L->getHeader()) {
            for (Instruction &I : *Header) {
                if (BranchInst *BI = dyn_cast<BranchInst>(&I)) {
                    if (BI->isConditional()) {
                        Value *Condition = BI->getCondition();
                        std::set<Value*> influencingInputs;
                        findInfluencingInputs(Condition, inputVariables, defUseChain, influencingInputs);
                        
                        if (!influencingInputs.empty()) {
                            errs() << "Loop termination condition at ";
                            if (const DILocation *Loc = I.getDebugLoc()) {
                                errs() << Loc->getFilename() << ":" << Loc->getLine();
                            }
                            errs() << " is influenced by input variables:\n";
                            for (Value *V : influencingInputs) {
                                errs() << "  ";
                                printVariableInfo(V, F);
                            }
                        }
                    }
                }
            }
        }
    }

    void findInfluencingInputs(Value *V, const std::set<Value*> &inputVariables,
                               const std::unordered_map<Value*, std::set<Value*>> &defUseChain,
                               std::set<Value*> &influencingInputs) {
        std::queue<Value*> workList;
        std::set<Value*> visited;
        workList.push(V);

        while (!workList.empty()) {
            Value *current = workList.front();
            workList.pop();

            if (visited.insert(current).second) {
                if (inputVariables.count(current) > 0) {
                    influencingInputs.insert(current);
                }

                if (Instruction *I = dyn_cast<Instruction>(current)) {
                    for (Use &U : I->operands()) {
                        workList.push(U.get());
                    }
                }

                auto it = defUseChain.find(current);
                if (it != defUseChain.end()) {
                    for (Value *user : it->second) {
                        workList.push(user);
                    }
                }
            }
        }
    }

    void printVariableInfo(Value *V, Function &F) {
        if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
            if (DbgDeclareInst *DDI = findDbgDeclare(AI, F)) {
                if (DILocalVariable *DIVar = DDI->getVariable()) {
                    errs() << "Variable: " << DIVar->getName() 
                           << ", Line: " << DDI->getDebugLoc().getLine() << "\n";
                    return;
                }
            }
        }
        errs() << *V << "\n";
    }

    DbgDeclareInst* findDbgDeclare(Value* AI, Function &F) {
        for (auto &BB : F) {
            for (auto &I : BB) {
                if (DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(&I)) {
                    if (DDI->getAddress() == AI) {
                        return DDI;
                    }
                }
            }
        }
        return nullptr;
    }
};

// Plugin registration function
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "KeyPointsPass", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "key-points-pass") {
                        FPM.addPass(KeyPointsPass());
                        return true;
                    }
                    return false;
                });
        }
    };
}
