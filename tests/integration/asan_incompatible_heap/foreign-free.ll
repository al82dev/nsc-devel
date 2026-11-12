target triple = "x86_64-pc-windows-msvc"
declare ptr @GetProcessHeap()
declare ptr @HeapAlloc(ptr, i32, i64)
declare i32 @nsc_asan_heap_free(ptr, i32, ptr)
define i32 @main() {
entry:
  %heap = call ptr @GetProcessHeap()
  %memory = call ptr @HeapAlloc(ptr %heap, i32 0, i64 24)
  %exists = icmp ne ptr %memory, null
  br i1 %exists, label %probe, label %failed
probe:
  %result = call i32 @nsc_asan_heap_free(ptr %heap, i32 0, ptr %memory)
  ret i32 3
failed:
  ret i32 2
}
