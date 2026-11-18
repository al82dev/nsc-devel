target triple = "x86_64-pc-windows-msvc"
declare ptr @GetProcessHeap()
declare ptr @nsc_asan_heap_alloc(ptr, i32, i64)
declare ptr @nsc_asan_heap_realloc(ptr, i32, ptr, i64)
define i32 @main() {
entry:
  %heap = call ptr @GetProcessHeap()
  %memory = call ptr @nsc_asan_heap_alloc(ptr %heap, i32 0, i64 8)
  %result = call ptr @nsc_asan_heap_realloc(ptr %heap, i32 8, ptr %memory, i64 16)
  ret i32 3
}
