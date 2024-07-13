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

llvm::GlobalVariable* defineGlobalVariable(llvm::Type *type, std::string name, llvm::Constant *init) {
  TheModule->getOrInsertGlobal(name, type);
  auto *globalVar = TheModule->getNamedGlobal(name);
  globalVar->setInitializer(init);
  globalVar->setDSOLocal(true);
  return globalVar;
}

llvm::GlobalVariable* defineGlobalVariable(std::string name, llvm::Constant *init) {
  return defineGlobalVariable(init->getType(), name, init);
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

  auto constantVar = defineGlobalVariable(ty, constVarName, init);

  constantVar->setConstant(true);
  constantVar->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
}

void emitStringPtr(std::string content, std::string name) {
  auto strPtr = Builder->CreateGlobalStringPtr(content, "." + name);
}

/**
 * char i_8 = 1;
 * short i_16 = 2;
 * int i_32 = 3;
 * long i_64 = 4;
 */
void emitIntegers() {
  // char i_8 = 1;
  defineGlobalVariable(Builder->getInt8Ty(), "i_8", Builder->getInt8(1));

  // short i_16 = 2;
  defineGlobalVariable(Builder->getInt16Ty(), "i_16", Builder->getInt16(2));

  // int i_32 = 3;
  defineGlobalVariable(Builder->getInt32Ty(), "i_32", Builder->getInt32(3));

  // long i_64 = 4;
  defineGlobalVariable(Builder->getInt64Ty(), "i_64", Builder->getInt64(4));

  // unsigned char ui_8 = 1
  defineGlobalVariable(Builder->getInt8Ty(), "ui_8", Builder->getInt8(1));

  // unsigned int ui_32 = 3;
  defineGlobalVariable(Builder->getInt32Ty(), "ui_32", Builder->getInt32(3));

  // int i32_1 = 1;
  defineGlobalVariable("i32_1", Builder->getInt32(1));
 // int i32_2 = 2;
  defineGlobalVariable("i32_2", Builder->getInt32(2));

  // unsigned int ui32_1 = 1;
  defineGlobalVariable("ui32_1", Builder->getInt32(1));
  // unsigned int ui32_2 = 2;
  defineGlobalVariable("ui32_2", Builder->getInt32(2));
}

void emitFloats() {
  // float f = 1.0;
  defineGlobalVariable(Builder->getFloatTy(), "f", llvm::ConstantFP::get(Builder->getFloatTy(), 1.0));
  
  // double d = 2.0
  defineGlobalVariable(Builder->getDoubleTy(), "df",llvm:: ConstantFP::get(Builder->getDoubleTy(), 2.0));
  
  //long double ld = 3.0;
  auto ldType = llvm::Type::getX86_FP80Ty(*TheContext);
  defineGlobalVariable(ldType, "ld", llvm::ConstantFP::get(ldType, 3.0));

  // float f_1 = 1.0;
  defineGlobalVariable("f_1", llvm::ConstantFP::get(Builder->getFloatTy(), 1.0));
  
  // float f_2 = 2.0
  defineGlobalVariable("f_2", llvm::ConstantFP::get(Builder->getFloatTy(), 2.0));
}

// int arr[] = { 1, 2, 3, 4 };
void emitArray() {
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

  defineGlobalVariable(arrType, "arr", c);
}

/**
 * struct point { int x; int y; };
 * struct point point = { 1, 2 };
 */
void emitStruct() {
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

  defineGlobalVariable(structTy, "pointer", c);
}

/**
 * union ab  {  int a;  float b; };
 * union ab u = { 1 };
 */
void emitUnion() {
  // union ab  {  int a;  float b; };
  llvm::StructType *structTy = llvm::StructType::create(*TheContext, "union.ab");
  structTy->setBody(Builder->getInt32Ty());

  // union ab u = { 1 };
  int num = 1;
  llvm::SmallVector<llvm::Constant *, 16> list;
  list.reserve(num);
  list.push_back(Builder->getInt32(1));

  llvm::Constant *c = llvm::ConstantStruct::get(structTy, list);
  defineGlobalVariable(structTy, "u", c);
}

void emitPointer() {
  // int *i_p;
  auto pointerTy = llvm::PointerType::get(Builder->getInt32Ty(), 0);
  auto c = llvm::Constant::getNullValue(pointerTy);
  defineGlobalVariable(pointerTy, "i_p", c);

  // char *c_p;
  auto charPtrTy = llvm::PointerType::get(Builder->getInt8Ty(), 0);
  auto cCharPtr = llvm::Constant::getNullValue(charPtrTy);
  defineGlobalVariable(charPtrTy, "c_p", cCharPtr);
}

// char *str = "hello\n";
void emitConstString() {
  emitStringPtr("hello", "str");
}

llvm::Value* emitMainFunctionStatementList() {
  auto siV1 = emitLoadGlobalVar("i32_1");
  auto siV2 = emitLoadGlobalVar("i32_2");

  // i32_1 > i32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_SGT, siV1, siV2);
  // i32_1 >= i32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_SGE, siV1, siV2);
  // i32_1 < i32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_SLT, siV1, siV2);
  // i32_1 <= i32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_SLE, siV1, siV2);
  // i32_1 == i32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_EQ, siV1, siV2);
  // i32_1 != i32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_NE, siV1, siV2);

  auto usiV1 = emitLoadGlobalVar("ui32_1");
  auto usiV2 = emitLoadGlobalVar("ui32_2");

  // ui32_1 > ui32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_SGT, usiV1, usiV2);
  // ui32_1 >= ui32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_SGE, usiV1, usiV2);
  // ui32_1 < ui32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_SLT, usiV1, usiV2);
  // ui32_1 <= ui32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_SLE, usiV1, usiV2);
  // ui32_1 == ui32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_EQ, usiV1, usiV2);
  // ui32_1 != ui32_2;
  Builder->CreateICmp(llvm::ICmpInst::ICMP_NE, usiV1, usiV2);

  auto fV1 = emitLoadGlobalVar("f_1");
  auto fV2 = emitLoadGlobalVar("f_2");

  // f_1 > f_2;
  Builder->CreateFCmp(llvm::FCmpInst::FCMP_OGT, fV1, fV2);
  // f_1 >= f_2;
  Builder->CreateFCmp(llvm::FCmpInst::FCMP_OGE, fV1, fV2);
  // f_1 < f_2;
  Builder->CreateFCmp(llvm::FCmpInst::FCMP_OLT, fV1, fV2);
  // f_1 <= f_2;
  Builder->CreateFCmp(llvm::FCmpInst::FCMP_OLE, fV1, fV2);
  // f_1 == f_2;
  Builder->CreateFCmp(llvm::FCmpInst::FCMP_ONE, fV1, fV2);
  // f_1 != f_2;
  Builder->CreateFCmp(llvm::FCmpInst::FCMP_UNE, fV1, fV2);

  // return i32_1;
  return siV1;
}

void emitProgram() {
  emitIntegers();
  emitFloats();

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
