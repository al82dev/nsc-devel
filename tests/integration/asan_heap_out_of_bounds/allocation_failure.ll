target triple = "x86_64-pc-windows-msvc"
declare ptr @GetProcessHeap()
declare ptr @HeapAlloc(ptr, i32, i64)
declare ptr @nsc_asan_heap_alloc(ptr, i32, i64)
declare ptr @nsc_asan_heap_realloc(ptr, i32, ptr, i64)
declare i32 @nsc_asan_heap_free(ptr, i32, ptr)
define i32 @main() {
entry:
  %heap = call ptr @GetProcessHeap()
  %ordinary = call ptr @HeapAlloc(ptr %heap, i32 0, i64 -1)
  %ordinary_failed = icmp eq ptr %ordinary, null
  br i1 %ordinary_failed, label %allocate_too_large, label %bad
allocate_too_large:
  %too_large = call ptr @nsc_asan_heap_alloc(ptr %heap, i32 0, i64 -1)
  %large_failed = icmp eq ptr %too_large, null
  br i1 %large_failed, label %allocate_valid, label %bad
allocate_valid:
  %memory = call ptr @nsc_asan_heap_alloc(ptr %heap, i32 0, i64 8)
  %valid = icmp ne ptr %memory, null
  br i1 %valid, label %reallocate_too_large, label %bad
reallocate_too_large:
  store i32 42, ptr %memory
  %resized = call ptr @nsc_asan_heap_realloc(ptr %heap, i32 0, ptr %memory, i64 -1)
  %resize_failed = icmp eq ptr %resized, null
  br i1 %resize_failed, label %verify_original, label %bad
verify_original:
  %value = load i32, ptr %memory
  %released = call i32 @nsc_asan_heap_free(ptr %heap, i32 0, ptr %memory)
  %value_ok = icmp eq i32 %value, 42
  %released_ok = icmp eq i32 %released, 1
  %ok = and i1 %value_ok, %released_ok
  %result = select i1 %ok, i32 42, i32 1
  ret i32 %result
bad:
  ret i32 1
}
