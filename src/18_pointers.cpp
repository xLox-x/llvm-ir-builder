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

// emit function statement list
typedef llvm::Value* (*EmitStatementList)(llvm::Function *);
static std::map<std::string, EmitStatementList> funImplMap;

void registerFunctionProto() {
  // int main(int)
  funProtoMap["main"] = {
    Builder->getInt32Ty(),
    {},
    false,
  };

  // int sum(int, int)
  funProtoMap["sum"] = {
    Builder->getInt32Ty(),
    { Builder->getInt32Ty(), Builder->getInt32Ty() },
    false,
  };

  // int printf(const char *format, ...)
  funProtoMap["printf"] = {
    Builder->getInt32Ty(),
    { Builder->getInt8Ty()->getPointerTo() },
    true,
  };

  auto i32PtrTy = Builder->getInt32Ty()->getPointerTo();
  // void swap_ptr(int *, int *)
  funProtoMap["swap_ptr"] = {
    Builder->getVoidTy(),
    { i32PtrTy, i32PtrTy },
    false,
  };
}

llvm::Value* emitMainStatementList(llvm::Function *);
llvm::Value* emitSumStatementList(llvm::Function *);
llvm::Value* emitSwapPtrStatementList(llvm::Function *);

void registerFunctionImpl() {
  funImplMap["main"] = emitMainStatementList;
  funImplMap["sum"] = emitSumStatementList;
  funImplMap["swap_ptr"] = emitSwapPtrStatementList;
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

llvm::BasicBlock* createBB(llvm::Function *fn, std::string name) {
  return llvm::BasicBlock::Create(*TheContext, name, fn);
}

void emitFunctionBody(llvm::Function *fn, std::string name) {
  // Create entry basic block
  auto *entry = createBB(fn, "entry");
  Builder->SetInsertPoint(entry);

  auto emitter = funImplMap[name];
  auto value = emitter(fn);

  // emit return
  emitReturn(fn->getReturnType(), value);
}

void defineFunction(std::string name) {
  // Function must be declated before define
  auto* fn = TheModule->getFunction(name);
  emitFunctionBody(fn, name);
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

void emitStoreGlobalVar(llvm::Value *value, std::string name) {
  auto globalVar = TheModule->getGlobalVariable(name);
  emitAssign(globalVar, value);
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

llvm::Constant* emitStringPtr(std::string content, std::string name) {
  auto strPtr = Builder->CreateGlobalString(content, "." + name);
  return strPtr;
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

llvm::Value* genIncrement(llvm::Value *left, int step) {
  // Temporary variables/Registers
  auto type = left->getType()->getNonOpaquePointerElementType();
  auto valueL = Builder->CreateLoad(type, left);
  auto valueR = Builder->getInt32(step);
  auto value = Builder->CreateNSWAdd(valueL, valueR);
  return value;
}

llvm::Value *getRValue(llvm::Value *value, llvm::Type *type) {
  auto address = Builder->CreateLoad(type, value);
  auto baseType = type->getNonOpaquePointerElementType();
  return Builder->CreateLoad(baseType, address);
}

llvm::Type* getPointerType(llvm::Type *baseType) {
  return llvm::PointerType::get(baseType, 0);
}

llvm::Value* emitMainStatementList(llvm::Function *fn) {
  std::vector<llvm::Value*> argsV;
  argsV.push_back(TheModule->getGlobalVariable("start"));
  argsV.push_back(TheModule->getGlobalVariable("end"));
  // swap_ptr(&start, &end);
  auto targetFn = TheModule->getFunction("swap_ptr");
  Builder->CreateCall(targetFn, argsV);
  auto start = emitLoadGlobalVar("start");
  // return result;
  return start;
}

llvm::Value* emitSumStatementList(llvm::Function *fn) {
  // alloca params
  auto ty = Builder->getInt32Ty();
  auto tmpX = Builder->CreateAlloca(ty);
  auto tmpY = Builder->CreateAlloca(ty);

  // store params to stack
  auto AI = fn->arg_begin();
  auto argX = AI++;
  auto argY = AI;
  Builder->CreateStore(argX, tmpX);
  Builder->CreateStore(argY, tmpY);

  // printf("result:%d\n", result);
  auto str = emitStringPtr("result:%d\n", "str");
  auto result = emitLoadGlobalVar("result");
  
  std::vector<llvm::Value *> indexValues;
  indexValues.push_back(Builder->getInt64(0));
  indexValues.push_back(Builder->getInt64(0));
  auto strAddr = Builder->CreateInBoundsGEP(str->getType()->getNonOpaquePointerElementType(),
    str, 
    indexValues 
  );
  std::vector<llvm::Value *> argsV;
  argsV.push_back(strAddr),
  argsV.push_back(result);
  auto printfFn = TheModule->getFunction("printf");
  Builder->CreateCall(printfFn, argsV);
  // x + y;
  auto valueL = Builder->CreateLoad(ty, tmpX);
  auto valueR = Builder->CreateLoad(ty, tmpY);

  auto value = Builder->CreateNSWAdd(valueL, valueR);
  // return result;
  return value;
}

llvm::Value *emitSwapPtrStatementList(llvm::Function *fn) {
    // alloca params
  auto baseType = Builder->getInt32Ty();
  auto ty = getPointerType(baseType);
  // int *alloca_x, *alloca_y;
  auto *tmpX = Builder->CreateAlloca(ty, nullptr, "param_x");
  auto *tmpY = Builder->CreateAlloca(ty, nullptr, "param_y");
  // int temp
  auto temp = Builder->CreateAlloca(baseType, nullptr, "temp");

  // store args on stack
  auto AI = fn->arg_begin();
  auto argX = AI++;
  auto argY = AI;
  Builder->CreateStore(argX, tmpX);
  Builder->CreateStore(argY, tmpY);

  // get r-value of *x;
  auto xValue = getRValue(tmpX, ty);
  // temp = *x;
  Builder->CreateStore(xValue, temp);

  // get r-value of *y
  auto yValue = getRValue(tmpY, ty);
  // get l-value of *x
  auto xAddress = Builder->CreateLoad(ty, tmpX);
  // *x = *y
  Builder->CreateStore(yValue, xAddress);

  // *y = temp
  auto tempValue = Builder->CreateLoad(baseType, temp);
  auto yAddress = Builder->CreateLoad(ty, tmpY);
  Builder->CreateStore(tempValue, yAddress);

  return nullptr;
}

void emitProgram() {
  emitIntegers();

  declareFunction("printf");

  declareFunction("swap_ptr");
  defineFunction("swap_ptr");

  declareFunction("main");
  defineFunction("main");
}

int main(int argc, char *argv[]) {
  initializeModule();

  auto targetTriple = llvm::sys::getDefaultTargetTriple();
  TheModule->setTargetTriple(targetTriple);

  registerFunctionProto();
  registerFunctionImpl();

  emitProgram();

  TheModule->print(llvm::outs(), nullptr);

  saveModuleIRToFile("./out.ll");
  return 0;
}
