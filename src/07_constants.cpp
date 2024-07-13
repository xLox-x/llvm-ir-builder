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
  auto* func = TheModule->getFunction(name);
  if (func == nullptr) {
    FunProto funProto = funProtoMap[name];
    auto* funcType = llvm::FunctionType::get(funProto.returnType, funProto.params, funProto.isVarArg);
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

llvm::Value* emitMainFunctionStatementList();

void emitFunctionBody(llvm::Function *fn) {
  // Create entry basic block
  auto *entry = llvm::BasicBlock::Create(*TheContext, "entry", fn);
  Builder->SetInsertPoint(entry);

  // emit return
  auto *value = emitMainFunctionStatementList();
  emitReturn(fn->getReturnType(), value);
}

void defineFunction(std::string name) {
  // Function must be declated before define
  auto* fn = TheModule->getFunction(name);
  emitFunctionBody(fn);
  verifyFunction(*fn);
}

llvm::GlobalVariable* defineGlobalVariable(std::string name, llvm::Constant *init) {
  TheModule->getOrInsertGlobal(name, init->getType());
  auto *globalVar = TheModule->getNamedGlobal(name);
  globalVar->setInitializer(init);
  globalVar->setDSOLocal(true);
  return globalVar;
}

llvm::Value* emitLoadValue(llvm::GlobalVariable *value) {
  return Builder->CreateLoad(value->getInitializer()->getType(), value);
}

llvm::Value* emitLoadValue(llvm::Value *value) {
  auto baseType = value->getType()->getNonOpaquePointerElementType();
  return Builder->CreateLoad(baseType, value);
}

llvm::Value* emitLoadGlobalVar(std::string name) {
  auto globalVar = TheModule->getGlobalVariable(name);
  auto rValue = emitLoadValue(globalVar);
  return rValue;
}

void emitAssign(llvm::Value *left, llvm::Value *right) {
  Builder->CreateStore(right, left);
}

llvm::Value* emitStackLocalVariable(llvm::Type *type, std::string name) {
  return Builder->CreateAlloca(type, nullptr, name);
}

void emitConstant(llvm::Type *ty, std::string name, llvm::Constant *init) {
  // private constant format: @__constant.function_name.variable_name
  auto currentFunction = Builder->GetInsertBlock()->getParent();
  std::string funcName = currentFunction->getName().data();
  std::string constVarName = "__constant." + funcName + "."+ name;

  auto constantVar = defineGlobalVariable(constVarName, init);

  constantVar->setConstant(true);
  constantVar->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
}

void emitStringPtr(std::string content, std::string name) {
  auto strPtr = Builder->CreateGlobalStringPtr(content, "." + name);
}

// const int arr[] = { 1, 2, 3, 4 };
void emitConstArray() {
  // int arr[4];
  int num = 4;
  llvm::Type *ty = Builder->getInt32Ty();
  llvm::ArrayType *arrType = llvm::ArrayType::get(ty, num);

  llvm::SmallVector<llvm::Constant *, 16> list;
  list.reserve(num);
  list.push_back(Builder->getInt32(1));
  list.push_back(Builder->getInt32(2));
  list.push_back(Builder->getInt32(3));
  list.push_back(Builder->getInt32(4));

  llvm::Constant *c = llvm::ConstantArray::get(arrType, list);

  emitConstant(arrType, "int_array", c);
}

/**
 * struct point { int x; int y; };
 * const struct point point = { 1, 2 };
 */
void emitConstStruct() {
  // struct point { int x; int y; }
  llvm::StructType *structTy = llvm::StructType::create(*TheContext, "struct.point");
  structTy->setBody({Builder->getInt32Ty(), Builder->getInt32Ty()});

  // struct point  = { 11, 12 }
  int num = 2;
  llvm::SmallVector<llvm::Constant *, 16> list;
  list.reserve(num);
  list.push_back(Builder->getInt32(11));
  list.push_back(Builder->getInt32(12));
  
  llvm::Constant *c = llvm::ConstantStruct::get(structTy, list);

  emitConstant(structTy, "point", c);
}

// char *str = "hello\n";
void emitConstString() {
  emitStringPtr("hello", "string");
}

llvm::Value* emitMainFunctionStatementList() {
  emitConstArray();
  emitConstStruct();
  emitConstString();

  // %1 = load i32, i32 %local_b
  auto *value = emitLoadGlobalVar("global_a");
  // return a
  return value;
}

void emitProgram() {
  // int global_a = 1
  defineGlobalVariable("global_a", Builder->getInt32(1));

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

