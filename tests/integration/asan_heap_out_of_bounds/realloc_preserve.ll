; Exercise the generated ASan adapter's successful allocation, growth, shrink,
; data preservation, and final release on the process heap.
target triple = "x86_64-pc-windows-msvc"

declare ptr @GetProcessHeap()
declare ptr @nsc_asan_heap_alloc(ptr, i32, i64)
declare ptr @nsc_asan_heap_realloc(ptr, i32, ptr, i64)
declare i32 @nsc_asan_heap_free(ptr, i32, ptr)

define void @fill(ptr %memory) {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  %element = getelementptr i8, ptr %memory, i64 %i
  %value = trunc i64 %i to i8
  store i8 %value, ptr %element
  %next = add i64 %i, 1
  %finished = icmp eq i64 %next, 64
  br i1 %finished, label %done, label %loop
done:
  ret void
}

define i1 @matches(ptr %memory, i64 %count) {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %next, %advance ]
  %element = getelementptr i8, ptr %memory, i64 %i
  %actual = load i8, ptr %element
  %expected = trunc i64 %i to i8
  %equal = icmp eq i8 %actual, %expected
  br i1 %equal, label %advance, label %mismatch
advance:
  %next = add i64 %i, 1
  %finished = icmp eq i64 %next, %count
  br i1 %finished, label %matched, label %loop
matched:
  ret i1 true
mismatch:
  ret i1 false
}

define i32 @main() {
entry:
  %heap = call ptr @GetProcessHeap()
  %initial = call ptr @nsc_asan_heap_alloc(ptr %heap, i32 0, i64 64)
  %allocated = icmp ne ptr %initial, null
  br i1 %allocated, label %grow, label %allocation_failed
grow:
  call void @fill(ptr %initial)
  %grown = call ptr @nsc_asan_heap_realloc(ptr %heap, i32 0, ptr %initial, i64 8192)
  %growth_ok = icmp ne ptr %grown, null
  br i1 %growth_ok, label %check_grown, label %growth_failed
check_grown:
  %prefix_ok = call i1 @matches(ptr %grown, i64 64)
  br i1 %prefix_ok, label %shrink, label %grown_mismatch
shrink:
  %smaller = call ptr @nsc_asan_heap_realloc(ptr %heap, i32 0, ptr %grown, i64 32)
  %shrink_ok = icmp ne ptr %smaller, null
  br i1 %shrink_ok, label %check_smaller, label %shrink_failed
check_smaller:
  %remaining_ok = call i1 @matches(ptr %smaller, i64 32)
  %released = call i32 @nsc_asan_heap_free(ptr %heap, i32 0, ptr %smaller)
  %free_ok = icmp eq i32 %released, 1
  %ok = and i1 %remaining_ok, %free_ok
  %result = select i1 %ok, i32 42, i32 3
  ret i32 %result
growth_failed:
  call i32 @nsc_asan_heap_free(ptr %heap, i32 0, ptr %initial)
  ret i32 2
grown_mismatch:
  call i32 @nsc_asan_heap_free(ptr %heap, i32 0, ptr %grown)
  ret i32 4
shrink_failed:
  call i32 @nsc_asan_heap_free(ptr %heap, i32 0, ptr %grown)
  ret i32 5
allocation_failed:
  ret i32 1
}
