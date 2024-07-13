; ModuleID = 'while.c'
source_filename = "while.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@start = dso_local global i32 1, align 4
@end = dso_local global i32 2, align 4
@result = dso_local global i32 0, align 4

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %3 = load i32, i32* @start, align 4
  store i32 %3, i32* %2, align 4
  br label %4

4:                                                ; preds = %8, %0
  %5 = load i32, i32* %2, align 4
  %6 = load i32, i32* @end, align 4
  %7 = icmp sle i32 %5, %6
  br i1 %7, label %8, label %14

8:                                                ; preds = %4
  %9 = load i32, i32* @result, align 4
  %10 = load i32, i32* %2, align 4
  %11 = add nsw i32 %9, %10
  store i32 %11, i32* @result, align 4
  %12 = load i32, i32* %2, align 4
  %13 = add nsw i32 %12, 1
  store i32 %13, i32* %2, align 4
  br label %4

14:                                               ; preds = %4
  %15 = load i32, i32* @result, align 4
  ret i32 %15
}


