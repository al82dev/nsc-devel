; Deliberately bypasses the process heap while preserving the runtime's old heap imports.
target triple = "x86_64-pc-windows-msvc"
@arena = internal global [4096 x i8] zeroinitializer, align 16
define ptr @__nativesharp_alloc(i64 %size) {
entry:
  ret ptr @arena
}
