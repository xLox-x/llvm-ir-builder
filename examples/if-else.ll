; ModuleID = 'if-else.c'
source_filename = "if-else.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@i32_1 = dso_local global i32 1, align 4
@i32_2 = dso_local global i32 2, align 4
@result = dso_local global i32 0, align 4

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = load i32, i32* @i32_1, align 4
  %3 = load i32, i32* @i32_2, align 4
  %4 = icmp sgt i32 %2, %3
  br i1 %4, label %5, label %7

5:                                                ; preds = %0
  %6 = load i32, i32* @i32_1, align 4
  store i32 %6, i32* @result, align 4
  br label %9

7:                                                ; preds = %0
  %8 = load i32, i32* @i32_2, align 4
  store i32 %8, i32* @result, align 4
  br label %9

9:                                                ; preds = %7, %5
  %10 = load i32, i32* @result, align 4
  ret i32 %10
}
