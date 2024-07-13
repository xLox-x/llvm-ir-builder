; ModuleID = 'ir_builder'
source_filename = "ir_builder"
target triple = "x86_64-pc-linux-gnu"

%struct.point = type { i32, i32 }

declare dso_local i32 @printf(i8*, ...)

define dso_local void @swap_struct(%struct.point* %0) {
entry:
  %param_p = alloca %struct.point*, align 8
  %temp = alloca i32, align 4
  store %struct.point* %0, %struct.point** %param_p, align 8
  %1 = load %struct.point*, %struct.point** %param_p, align 8
  %2 = getelementptr inbounds %struct.point, %struct.point* %1, i32 0, i32 0
  %3 = load i32, i32* %2, align 4
  store i32 %3, i32* %temp, align 4
  %4 = load %struct.point*, %struct.point** %param_p, align 8
  %5 = getelementptr inbounds %struct.point, %struct.point* %4, i32 0, i32 1
  %6 = load i32, i32* %5, align 4
  %7 = load %struct.point*, %struct.point** %param_p, align 8
  %8 = getelementptr inbounds %struct.point, %struct.point* %7, i32 0, i32 0
  store i32 %6, i32* %8, align 4
  %9 = load i32, i32* %temp, align 4
  %10 = load %struct.point*, %struct.point** %param_p, align 8
  %11 = getelementptr inbounds %struct.point, %struct.point* %10, i32 0, i32 1
  store i32 %9, i32* %11, align 4
  ret void
}

define dso_local i32 @main() {
entry:
  %param_p = alloca %struct.point, align 8
  %0 = getelementptr inbounds %struct.point, %struct.point* %param_p, i32 0, i32 0
  store i32 10, i32* %0, align 4
  %1 = getelementptr inbounds %struct.point, %struct.point* %param_p, i32 0, i32 1
  store i32 20, i32* %1, align 4
  call void @swap_struct(%struct.point* %param_p)
  %2 = getelementptr inbounds %struct.point, %struct.point* %param_p, i32 0, i32 0
  %3 = load i32, i32* %2, align 4
  ret i32 %3
}
