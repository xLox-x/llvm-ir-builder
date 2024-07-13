#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Host.h"

#include <map>
#include <string>

using namespace llvm;

static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<Module> TheModule;
static std::unique_ptr<IRBuilder<>> Builder;

typedef struct FunProto {
  Type *returnType;
  ArrayRef<Type *> params;
  bool isVarArg;
} FunProto;

static std::map<std::string, FunProto> funProtoMap;

static void initializeModule() {
  // Open a new context and moduel
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("ir_builder", *TheContext);
  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

static void saveModuleIRToFile(const std::string& filename) {
  std::error_code errorCode;
  llvm::raw_fd_ostream out(filename, errorCode);
  TheModule->print(out, nullptr);
}

void registerFunctionProto() {
  auto int32Ty = Builder->getInt32Ty();
  funProtoMap["main"] = {
    int32Ty,
    {},
    false,
  };
}

Function *declareFunction(std::string name) {
  Function* func = TheModule->getFunction(name);
  if (func == nullptr) {
    FunProto funProto = funProtoMap[name];
    FunctionType* funcType = FunctionType::get(funProto.returnType, funProto.params, funProto.isVarArg);
    func = Function::Create(funcType, Function::ExternalLinkage, name, TheModule.get());
    func->setDSOLocal(true);
  }
  return func;
}

void emitReturn(Type *ty, Value *value) {
  if (ty->isVoidTy()) {
    Builder->CreateRetVoid();
  } else {
    Builder->CreateRet(value);
  }
}

static BasicBlock *createBB(Function *fn, std::string Name) {
  return BasicBlock::Create(*TheContext, Name, fn);
}

Value* emitMainFunctionStatementList(Function *);

void emitFunctionBody(Function *fn) {
  // Create entry basic block
  BasicBlock *entry = createBB(fn, "entry");
  Builder->SetInsertPoint(entry);

  Value *value = emitMainFunctionStatementList(fn);
  // emit return
  emitReturn(fn->getReturnType(), value);
}

void defineFunction(std::string name) {
  // Function must be declated before define
  Function* fn = TheModule->getFunction(name);
  emitFunctionBody(fn);
  verifyFunction(*fn);
}

GlobalVariable* defineGlobalVariable(std::string name, llvm::Constant *init) {
  TheModule->getOrInsertGlobal(name, init->getType());
  GlobalVariable *globalVar = TheModule->getNamedGlobal(name);
  globalVar->setInitializer(init);
  globalVar->setDSOLocal(true);
  return globalVar;
}

void emitIntegers() {
  // int start = 1;
  defineGlobalVariable("start", Builder->getInt32(1));
  // int end = 10;
  defineGlobalVariable("end", Builder->getInt32(10));
  // int result = 0;
  defineGlobalVariable("result", Builder->getInt32(0));
}

void emitFloats() {
  // float f_1 = 1.0;
  defineGlobalVariable("f_1", ConstantFP::get(Builder->getFloatTy(), 1.0));
  
  // float f_2 = 2.0
  defineGlobalVariable("f_2", ConstantFP::get(Builder->getFloatTy(), 2.0));
}

Value* emitLoadValue(GlobalVariable *value) {
  return Builder->CreateLoad(value->getInitializer()->getType(), value);
}

Value* emitLoadValue(Value *value) {
  auto baseType = value->getType()->getNonOpaquePointerElementType();
  return Builder->CreateLoad(baseType, value);
}

Value* emitLoadGlobalVar(std::string name) {
  auto globalVar = TheModule->getGlobalVariable(name);
  auto rValue = emitLoadValue(globalVar);
  return rValue;
}

void emitAssign(Value *left, Value *right) {
  Builder->CreateStore(right, left);
}

void emitStoreGlobalVar(Value *value, std::string name) {
  auto globalVar = TheModule->getGlobalVariable(name);
  emitAssign(globalVar, value);
}

Value* emitAllocaLocalVariable(Type *type, std::string name) {
  return Builder->CreateAlloca(type, nullptr, name);
}

Value* genIncrement(Value *left, int step) {
  // Temporary variables/Registers
  Type *type = left->getType()->getNonOpaquePointerElementType();
  Value *valueL = Builder->CreateLoad(type, left);
  Value *valueR = Builder->getInt32(step);
  Value *value = value = Builder->CreateNSWAdd(valueL, valueR);
  return value;
}

Value* emitMainFunctionStatementList(Function *fn) {
  BasicBlock *conditionBB = createBB(fn, "condition");
  BasicBlock *bodyBB = createBB(fn, "body");
  BasicBlock *endBB = createBB(fn, "end");

  // int index;
  Value *indexAddr = Builder->CreateAlloca(Builder->getInt32Ty(), nullptr, "index");

  // index = start;
  auto startV = emitLoadGlobalVar("start");
  Builder->CreateStore(startV, indexAddr);

  // goto for condition
  Builder->CreateBr(conditionBB);
  
  // condition bb 
  Builder->SetInsertPoint(conditionBB);
  // index <= start;
  auto indexV = emitLoadValue(indexAddr);
  auto endV = emitLoadGlobalVar("end");
  auto compare = Builder->CreateICmpSLE(indexV, endV);
  Builder->CreateCondBr(compare, bodyBB, endBB);

  // body bb
  Builder->SetInsertPoint(bodyBB);
  // result = result + index;
  auto resultV = emitLoadGlobalVar("result");
  indexV = emitLoadValue(indexAddr);
  auto sum = Builder->CreateNSWAdd(resultV, indexV);
  emitStoreGlobalVar(sum, "result");
  // index = index + 1
  Value *incVal = genIncrement(indexAddr, 1);
  Builder->CreateStore(incVal, indexAddr);
  Builder->CreateBr(conditionBB);

  // end
  Builder->SetInsertPoint(endBB);
  auto value = emitLoadGlobalVar("result");
  // return result;
  return value;
}

void emitProgram() {
  emitIntegers();

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
