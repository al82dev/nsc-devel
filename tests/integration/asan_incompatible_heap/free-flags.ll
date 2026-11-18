target triple = "x86_64-pc-windows-msvc"
declare ptr @GetProcessHeap()
declare ptr @nsc_asan_heap_alloc(ptr, i32, i64)
declare i32 @nsc_asan_heap_free(ptr, i32, ptr)
define i32 @main() {
entry:
  %heap = call ptr @GetProcessHeap()
  %memory = call ptr @nsc_asan_heap_alloc(ptr %heap, i32 0, i64 8)
  %result = call i32 @nsc_asan_heap_free(ptr %heap, i32 8, ptr %memory)
  ret i32 3
}
