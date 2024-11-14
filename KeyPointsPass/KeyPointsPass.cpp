#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"

using namespace llvm;

class KeyPointsPass : public PassInfoMixin<KeyPointsPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        errs() << "Analyzing : " << F.getName() << "\n";
        int count = 1;

        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *callInst = dyn_cast<CallInst>(&I)) {
                    if (!callInst->getCalledFunction()) { // Indirect call
                        Value *calledOperand = callInst->getCalledOperand();

                        // KEY POINT DETECTION

                        // Log function pointer invocation with line number
                        if (const DILocation *Loc = I.getDebugLoc()) {
                            errs() << Loc->getFilename() << ":" << Loc->getLine() << " - ";
                        }
                        errs() << "*func_" << calledOperand->stripPointerCasts() << "\n";
                    }
                } else if (auto *branchInst = dyn_cast<BranchInst>(&I)) {
                    // KEY POINT DETECTION
                    
                    // Log branch execution with line number
                    if (branchInst->isConditional()) {
                        if (const DILocation *Loc = I.getDebugLoc()) {
                            std::string branchID = "br_" + std::to_string(count);
                            errs() << Loc->getFilename() << ":" << Loc->getLine() << " - " << branchID << "\n";
                            count++;
                        }
                    }
                }
            }
        }

        return PreservedAnalyses::all();
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
