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

llvm::Value *getStructElementAddr(int index, llvm::Value *ptrval);

llvm::Type* emitPointType() {
  auto element_ty = Builder->getInt32Ty();
  auto point_ty = llvm::StructType::create(*TheContext, "struct.point");
  point_ty->setBody({ element_ty, element_ty });
  return point_ty;
}

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

  auto i32Ty = Builder->getInt32Ty();
  // void swap_arr(int[], int, int)
  funProtoMap["swap_array"] = {
    Builder->getVoidTy(),
    { i32PtrTy, i32Ty, i32Ty },
    false,
  };

  auto pointTy = emitPointType();
  // void swap_point(struct point *)
  funProtoMap["swap_point"] = {
    Builder->getVoidTy(),
    { pointTy->getPointerTo() },
    false,
  };   
}

llvm::Value* emitMainStatementList(llvm::Function *);
llvm::Value* emitSumStatementList(llvm::Function *);
llvm::Value* emitSwapPtrStatementList(llvm::Function *);
llvm::Value* emitSwapArrayStatementList(llvm::Function *);
llvm::Value* emitSwapPointStatementList(llvm::Function *);

void registerFunctionImpl() {
  funImplMap["main"] = emitMainStatementList;
  funImplMap["sum"] = emitSumStatementList;
  funImplMap["swap_ptr"] = emitSwapPtrStatementList;
  funImplMap["swap_array"] = emitSwapArrayStatementList;
  funImplMap["swap_point"] = emitSwapPointStatementList;
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

llvm::Value* emitPoint() {
  auto *point_ty = llvm::StructType::getTypeByName(*TheContext, "struct.point");  
  // struct point p;
  auto tmp_p = Builder->CreateAlloca(point_ty, nullptr, "param_p");
  // p.x = 10;
  auto p_x = getStructElementAddr(0, tmp_p);
  Builder->CreateStore(Builder->getInt32(10), p_x);
  // p.y = 20;
  auto p_y = getStructElementAddr(1, tmp_p);
  Builder->CreateStore(Builder->getInt32(20), p_y);
  return tmp_p;
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

llvm::Value *getElementAddr(llvm::Value *arrAddr, llvm::Value *indexAddr) {
  auto ty = arrAddr->getType()->getNonOpaquePointerElementType();
  auto *baseType = ty->getNonOpaquePointerElementType();
  auto arr = Builder->CreateLoad(ty, arrAddr);

  auto indexType = indexAddr->getType()->getNonOpaquePointerElementType();
  auto indexValue = Builder->CreateLoad(indexType, indexAddr);
  auto indexTy64Value = Builder->CreateSExt(indexValue, Builder->getInt64Ty());

  std::vector<llvm::Value *> IndexValues;
  IndexValues.push_back(indexTy64Value);
  llvm::Value *target = Builder->CreateInBoundsGEP(arr->getType()->getNonOpaquePointerElementType(), arr, IndexValues);

  return target;
}

/// Return the value offset.
static llvm::Constant *getStructOffset(long offset) {
  return llvm::ConstantInt::get(Builder->getInt32Ty(), offset);
}

llvm::Value *getStructElementAddr(int index, llvm::Value *ptrval) { 
  std::vector<llvm::Value *> IndexValues;
  IndexValues.push_back(getStructOffset(0));
  IndexValues.push_back(getStructOffset(index));
  llvm::Value *target = Builder->CreateInBoundsGEP(ptrval->getType()->getNonOpaquePointerElementType(), ptrval, IndexValues);
  return target;
}

llvm::Value *getStructElementRValue(llvm::Value *structAlloca, int index) {
  auto ty = structAlloca->getType()->getNonOpaquePointerElementType();
  auto structAddr = Builder->CreateLoad(ty, structAlloca);
  auto elementAddr = getStructElementAddr(index, structAddr);
  auto structType = llvm::dyn_cast<llvm::StructType>(structAddr->getType()->getNonOpaquePointerElementType());
  auto elementType = structType->getTypeAtIndex(index);
  return Builder->CreateLoad(elementType, elementAddr);
}

llvm::Value *getStructElementLValue(llvm::Value *structAlloca, int index) {
  auto ty = structAlloca->getType()->getNonOpaquePointerElementType();
  auto structAddr = Builder->CreateLoad(ty, structAlloca);
  auto elementAddr = getStructElementAddr(index, structAddr);
  return elementAddr;
}

llvm::Type* getPointerType(llvm::Type *baseType) {
  return llvm::PointerType::get(baseType, 0);
}

/// Return the value offset.
llvm::Constant *getOffset(long offset) {
  return llvm::ConstantInt::get(Builder->getInt64Ty(), offset);
}

llvm::Value *gen_get_member_ptr(int index, llvm::Value *ptrval) { 
  std::vector<llvm::Value *> IndexValues;
  IndexValues.push_back(getOffset(0));
  IndexValues.push_back(getOffset(index));
  llvm::Value *target = Builder->CreateInBoundsGEP(ptrval->getType()->getNonOpaquePointerElementType(), ptrval, IndexValues);
  return target;
}

llvm::Value* emitMainStatementList(llvm::Function *fn) {
  // &point
  auto pointAddr = emitPoint();
  std::vector<llvm::Value*> argsV;
  argsV.push_back(pointAddr);

  // swap_point(&point);
  auto targetFn = TheModule->getFunction("swap_point");
  Builder->CreateCall(targetFn, argsV);

  // return point.x;
  auto pointX = getStructElementAddr(0, pointAddr);
  auto result = Builder->CreateLoad(fn->getReturnType(), pointX);
  return result;
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
llvm::Value *emitSwapArrayStatementList(llvm::Function *fn) {
    // alloca params
  auto baseType = Builder->getInt32Ty();
  auto ty = getPointerType(baseType);
  // int *alloca_x, *alloca_y;
  auto tmpArr = Builder->CreateAlloca(ty, nullptr, "param_arr");
  auto tmpX = Builder->CreateAlloca(baseType, nullptr, "param_x");
  auto tmpY = Builder->CreateAlloca(baseType, nullptr, "param_y");
  // int temp
  auto temp = Builder->CreateAlloca(baseType, nullptr, "temp");

  // store args on stack
  auto AI = fn->arg_begin();
  auto argArr = AI++;
  auto argX = AI++;
  auto argY = AI;
  Builder->CreateStore(argArr, tmpArr);
  Builder->CreateStore(argY, tmpY);
  Builder->CreateStore(argX, tmpX);
  
  // get l-value of arr[x]
  auto arr_x_addr = getElementAddr(tmpArr, tmpX);
  // get r-value of arr[x]
  auto arr_x_value = Builder->CreateLoad(baseType, arr_x_addr);
  // temp = arr[x]
  Builder->CreateStore(arr_x_value, temp);

  // get r-value of arr[y]
  auto arr_y_addr = getElementAddr(tmpArr, tmpY);
  auto arr_y_value = Builder->CreateLoad(baseType, arr_y_addr);
  // get l-value of arr[x]
  auto arr_x_addr_1 = getElementAddr(tmpArr, tmpX);
  // arr[x] = arr[y]
  Builder->CreateStore(arr_y_value, arr_x_addr_1);

  // arr[y] = temp
  auto temp_value = Builder->CreateLoad(baseType, temp);
  // get l-value of arr[y]
  auto arr_y_addr_1 = getElementAddr(tmpArr, tmpY);
  Builder->CreateStore(temp_value, arr_y_addr_1);
  return nullptr;
}

llvm::Value* emitSwapPointStatementList(llvm::Function *fn) {
  // alloca params
  auto baseType = Builder->getInt32Ty();
  auto ty = llvm::StructType::getTypeByName(*TheContext, "struct.point");

  // struct *alloca_p;
  auto *tmpP = Builder->CreateAlloca(getPointerType(ty), nullptr, "param_p");
  // int temp;
  auto temp = Builder->CreateAlloca(baseType, nullptr, "temp");

  // store args on stack
  auto AI = fn->arg_begin();
  auto arg_1 = AI++;
  Builder->CreateStore(arg_1, tmpP);

  // get r-value of pointer->x
  auto p_x_rvalue = getStructElementRValue(tmpP, 0);

  // temp = p.x
  Builder->CreateStore(p_x_rvalue, temp);

  // get r-value of pointer->y
  auto p_y_rvalue = getStructElementRValue(tmpP, 1);

  // get l-value of pointer->x
  auto p_x_lvalue = getStructElementLValue(tmpP, 0);

  // p->x = p->y
  Builder->CreateStore(p_y_rvalue, p_x_lvalue);

  // get r-value of temp
  auto temp_rvalue = Builder->CreateLoad(baseType, temp);

  // get l-value of p->y
  auto p_y_lvalue = getStructElementLValue(tmpP, 1);

  // temp = p->y
  Builder->CreateStore(temp_rvalue, p_y_lvalue);

  return nullptr;
}

void emitProgram() {
  declareFunction("printf");

  declareFunction("swap_point");
  defineFunction("swap_point");

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
