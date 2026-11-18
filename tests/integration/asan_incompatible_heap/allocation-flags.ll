target triple = "x86_64-pc-windows-msvc"
declare ptr @GetProcessHeap()
declare ptr @nsc_asan_heap_alloc(ptr, i32, i64)
define i32 @main() {
entry:
  %heap = call ptr @GetProcessHeap()
  %result = call ptr @nsc_asan_heap_alloc(ptr %heap, i32 8, i64 8)
  ret i32 3
}
