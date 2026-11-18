target triple = "x86_64-pc-windows-msvc"
declare ptr @HeapCreate(i32, i64, i64)
declare ptr @nsc_asan_heap_alloc(ptr, i32, i64)
define i32 @main() {
entry:
  %heap = call ptr @HeapCreate(i32 0, i64 0, i64 0)
  %exists = icmp ne ptr %heap, null
  br i1 %exists, label %probe, label %failed
probe:
  %result = call ptr @nsc_asan_heap_alloc(ptr %heap, i32 0, i64 24)
  ret i32 3
failed:
  ret i32 2
}
