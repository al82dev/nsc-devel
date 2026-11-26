#pragma once
struct S { static int call(int (*fn)(int), int x) { return fn(x); } };
