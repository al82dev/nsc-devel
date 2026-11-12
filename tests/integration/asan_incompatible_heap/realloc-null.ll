target triple = "x86_64-pc-windows-msvc"
declare ptr @GetProcessHeap()
declare ptr @nsc_asan_heap_realloc(ptr, i32, ptr, i64)
define i32 @main() {
entry:
  %heap = call ptr @GetProcessHeap()
  %result = call ptr @nsc_asan_heap_realloc(ptr %heap, i32 0, ptr null, i64 8)
  ret i32 3
}
