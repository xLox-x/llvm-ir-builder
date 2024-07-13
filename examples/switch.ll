; ModuleID = 'switch.c'
source_filename = "switch.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@level = dso_local global i32 1, align 4
@result = dso_local global i32 0, align 4

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = load i32, i32* @level, align 4
  switch i32 %2, label %5 [
    i32 1, label %3
    i32 2, label %4
  ]

3:                                                ; preds = %0
  store i32 90, i32* @result, align 4
  br label %6

4:                                                ; preds = %0
  store i32 80, i32* @result, align 4
  br label %6

5:                                                ; preds = %0
  store i32 70, i32* @result, align 4
  br label %6

6:                                                ; preds = %5, %4, %3
  %7 = load i32, i32* @result, align 4
  ret i32 %7
}