; ModuleID = 'for.c'
source_filename = "for.c"
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

4:                                                ; preds = %12, %0
  %5 = load i32, i32* %2, align 4
  %6 = load i32, i32* @end, align 4
  %7 = icmp sle i32 %5, %6
  br i1 %7, label %8, label %15

8:                                                ; preds = %4
  %9 = load i32, i32* @result, align 4
  %10 = load i32, i32* %2, align 4
  %11 = add nsw i32 %9, %10
  store i32 %11, i32* @result, align 4
  br label %12

12:                                               ; preds = %8
  %13 = load i32, i32* %2, align 4
  %14 = add nsw i32 %13, 1
  store i32 %14, i32* %2, align 4
  br label %4

15:                                               ; preds = %4
  %16 = load i32, i32* @result, align 4
  ret i32 %16
}

