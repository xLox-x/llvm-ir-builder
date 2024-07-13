#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Host.h"

#include <map>
#include <string>

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;

static void initializeModule() {
  // Open a new context and moduel
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("ir_builder", *TheContext);
  // Create a new builder for the module.
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

static void saveModuleIRToFile(const std::string& filename) {
  std::error_code errorCode;
  llvm::raw_fd_ostream out(filename, errorCode);
  TheModule->print(out, nullptr);
}

typedef struct FunProto {
  llvm::Type *returnType;
  std::vector<llvm::Type *> params;
  bool isVarArg;
} FunProto;

static std::map<std::string, FunProto> funProtoMap;

void registerFunctionProto() {
  auto int32Ty = Builder->getInt32Ty();
  funProtoMap["main"] = {
    int32Ty,
    {},
    false,
  };
}

llvm::Function *declareFunction(std::string name) {
  auto func = TheModule->getFunction(name);
  if (func == nullptr) {
    auto funProto = funProtoMap[name];
    auto funcType = llvm::FunctionType::get(funProto.returnType, funProto.params, funProto.isVarArg);
    func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, name, TheModule.get());
    func->setDSOLocal(true);
  }
  return func;
}

void emitReturn(llvm::Type *ty, llvm::Value *value) {
  if (ty->isVoidTy()) {
    Builder->CreateRetVoid();
  } else {
    Builder->CreateRet(value);
  }
}

void emitFunctionBody(llvm::Function *fn) {
  // Create entry basic block
  auto *entry = llvm::BasicBlock::Create(*TheContext, "entry", fn);
  Builder->SetInsertPoint(entry);

  // emit return
  auto *value = Builder->getInt32(10);
  emitReturn(fn->getReturnType(), value);
}

void defineFunction(std::string name) {
  // Function must be declated before define
  auto* fn = TheModule->getFunction(name);
  emitFunctionBody(fn);
  verifyFunction(*fn);
}

void emitProgram() {
  declareFunction("main");
  defineFunction("main");
}

int main(int argc, char *argv[]) {
  initializeModule();

  auto targetTriple = llvm::sys::getDefaultTargetTriple();
  TheModule->setTargetTriple(targetTriple);

  registerFunctionProto();

  emitProgram();

  TheModule->print(llvm::outs(), nullptr);

  saveModuleIRToFile("./out.ll");
  return 0;
}

