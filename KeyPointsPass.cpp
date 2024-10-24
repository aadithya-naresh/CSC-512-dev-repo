#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

class KeyPointsPass : public PassInfoMixin<KeyPointsPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        outs() << "Analyzing function: " << F.getName() << "\n";
        int count = 1;

        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *callInst = dyn_cast<CallInst>(&I)) {
                    if (!callInst->getCalledFunction()) { // Indirect call
                        Value *calledOperand = callInst->getCalledOperand();
                        // Log function pointer invocation
                        errs() << "*func_" << calledOperand->stripPointerCasts() << "\n";
                    }
                } else if (auto *branchInst = dyn_cast<BranchInst>(&I)) {
                    // Log branch execution
                    if (branchInst->isConditional()) {
                        auto debugLoc = branchInst->getDebugLoc();
                        if (debugLoc) {
                            std::string branchID = "br_" + std::to_string(count);
                            count++;
                            for (unsigned i = 0; i < branchInst->getNumSuccessors(); ++i) {
                                // Print for each successor to simulate multiple iterations
                                errs() << branchID << "\n";
                            }
                        }
                    }
                }
            }
        }

        return PreservedAnalyses::all();
    }
};

// Plugin registration function
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "KeyPointsPass", LLVM_VERSION_STRING,
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
            }};
}