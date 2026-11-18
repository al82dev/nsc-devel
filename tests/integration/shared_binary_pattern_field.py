"""Exercise readonly match loans against an isolated binary Core SDK."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

PREFIX = '''using NativeSharp.Core;
struct Value { public long Data; public Value() { Data = 42l; } ~Value() { } }
class Holder { public FAMILY<Value> value; public FAMILY<Value> other;
 public void Reset() { value = SharedPointers.MAKE<Value>(); } }
class Outer { public Holder inner; public Outer() { inner = new Holder(); } }
class Deep { public Outer outer; public Deep() { outer = new Outer(); } }
'''
CASES = {
    'read': ('''class Program { public static int Main() {
 Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 2; } }''', None),
    'overwrite': ('''class Program { public static int Main() {
 Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { holder.value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'move': ('''class Program { public static int Main() {
 Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { FAMILY<Value> moved = move(holder.value); return (int)item.Data; } }
 return 0; } }''', 'cannot partially move owner field while a borrowed view is alive'),
    'alias': ('''class Program { public static int Main() {
 Holder holder = new Holder(); borrowed Holder alias = holder; holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { alias.value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'alias_control': ('''class Program { public static int Main() {
 Holder holder = new Holder(); borrowed Holder alias = holder;
 alias.value = SharedPointers.MAKE<Value>(); return alias.value.Get().Data == 42l ? 42 : 1;
 } }''', None),
    'call': ('''class Program { public static int Main() {
 Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { holder.Reset(); return (int)item.Data; } }
 return 0; } }''', 'cannot call potentially mutating method while a borrowed field view is alive'),
    'alias_call': ('''class Program { public static int Main() {
 Holder holder = new Holder(); borrowed Holder alias = holder; holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { alias.Reset(); return (int)item.Data; } }
 return 0; } }''', 'cannot call potentially mutating method while a borrowed field view is alive'),
    'other': ('''class Program { public static int Main() {
 Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { holder.other = SharedPointers.MAKE<Value>();
 return item.Data == 42l ? 42 : 1; } } return 0; } }''', None),
    'alias_other': ('''class Program { public static int Main() {
 Holder holder = new Holder(); borrowed Holder alias = holder; holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { alias.other = SharedPointers.MAKE<Value>();
 return item.Data == 42l ? 42 : 1; } } return 0; } }''', None),
    'independent_call': ('''class Program { public static int Main() {
 Holder holder = new Holder(); Holder other = new Holder(); holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { other.Reset(); return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'nested_read': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'nested_direct_delete': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete outer.inner; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_finished': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>(); int result = 0;
 match (outer.inner.value.Get()) { ref item => { result = (int)item.Data; } }
 delete outer.inner; return result; } }''', None),
    'nested_direct_delete_independent': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Outer other = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete other.inner; return (int)item.Data; } }
 return 0; } }''', None),
    'nested_direct_delete_alias': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Holder alias = outer.inner; outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_independent_alias': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Outer other = new Outer(); Holder alias = other.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', None),
    'nested_direct_delete_forwarded': ('''class Program { static Holder Identity(Holder value) { return value; }
 public static int Main() { Outer outer = new Outer(); Holder alias = Identity(outer.inner);
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_independent_forwarded': ('''class Program { static Holder Identity(Holder value) { return value; }
 public static int Main() { Outer outer = new Outer(); Outer other = new Outer(); Holder alias = Identity(other.inner);
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', None),
    'nested_direct_delete_static_alias': ('''class Registry { public static Holder Saved; }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 Holder alias = Registry.Saved; outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_static_field': ('''class Registry { public static Holder Saved; }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete Registry.Saved; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_destroy_static_field': ('''class Registry { public static Holder Saved; }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { destroy Registry.Saved; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_static_field_finished': ('''class Registry { public static Holder Saved; }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>(); int result = 0;
 match (outer.inner.value.Get()) { ref item => { result = (int)item.Data; } }
 delete Registry.Saved; return result; } }''', None),
    'nested_direct_delete_static_forwarded': ('''class Registry { public static Holder Saved;
 public static Holder Read() { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 Holder alias = Registry.Read(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_static_with_argument': ('''class Registry { public static Holder Saved;
 public static Holder Read(Holder ignored) { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Outer other = new Outer();
 Registry.Saved = outer.inner; Holder alias = Registry.Read(other.inner);
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_static_call_expression': ('''class Registry { public static Holder Saved;
 public static Holder Read() { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete Registry.Read(); return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_destroy_static_call_expression': ('''class Registry { public static Holder Saved;
 public static Holder Read() { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { destroy Registry.Read(); return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_destroy_static_alias': ('''class Registry { public static Holder Saved; }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 Holder alias = Registry.Saved; outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { destroy alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_static_reassigned': ('''class Registry { public static Holder Saved;
 public static Holder Read() { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 Holder alias = null; alias = Registry.Read(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_destroy_static_reassigned_alias': ('''class Registry { public static Holder Saved;
 public static Holder Read() { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 Holder first = null; first = Registry.Read(); Holder alias = first; outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { destroy alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_static_call_finished': ('''class Registry { public static Holder Saved;
 public static Holder Read() { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>(); int result = 0;
 match (outer.inner.value.Get()) { ref item => { result = (int)item.Data; } }
 delete Registry.Read(); return result; } }''', None),
    'nested_direct_destroy_reassigned': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Holder alias = null; alias = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { destroy alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_delete_independent_reassigned': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Outer other = new Outer(); Holder alias = null; alias = other.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', None),
    'nested_direct_destroy': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { destroy outer.inner; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_destroy_alias': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Holder alias = outer.inner; outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { destroy alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_direct_destroy_finished': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>(); int result = 0;
 match (outer.inner.value.Get()) { ref item => { result = (int)item.Data; } }
 destroy outer.inner; return result; } }''', None),
    'nested_direct_destroy_independent': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Outer other = new Outer(); Holder alias = other.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { destroy alias; return (int)item.Data; } }
 return 0; } }''', None),
    'nested_chained_view_read': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'nested_chained_view_overwrite': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 outer.inner.value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'nested_chained_view_finished': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => { if (item.Data != 42l) return 1; } }
 outer.inner.value = SharedPointers.MAKE<Value>(); return outer.inner.value.Get().Data == 42l ? 42 : 2;
 } }''', None),
    'nested_chained_view_move': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 FAMILY<Value> taken = move(outer.inner.value); return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_chained_view_alias': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Holder alias = outer.inner; outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 alias.value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_chained_view_parent': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 outer.inner = new Holder(); return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_chained_view_independent': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 outer.inner.other = SharedPointers.MAKE<Value>(); return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'nested_chained_view_delete': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 delete outer.inner; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_chained_view_destroy_alias': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Holder alias = outer.inner; outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 destroy alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_chained_view_delete_static': ('''class Registry { public static Holder Saved; }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => { delete Registry.Saved; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_chained_view_delete_static_reassigned': ('''class Registry { public static Holder Saved;
 public static Holder Read() { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 Holder alias = null; alias = Registry.Read(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_chained_view_delete_static_call': ('''class Registry { public static Holder Saved;
 public static Holder Read() { return Saved; } }
 class Program { public static int Main() { Outer outer = new Outer(); Registry.Saved = outer.inner;
 outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => { delete Registry.Read(); return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'nested_chained_view_call': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 outer.inner.Reset(); return (int)item.Data; } }
 return 0; } }''', 'while a borrowed field view is alive'),
    'nested_chained_view_write': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Borrow().Get()) { ref item => {
 item.Data = 9l; return (int)item.Data; } }
 return 0; } }''', 'left side of assignment is not assignable'),
    'indexed_read': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'indexed_overwrite': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { holders[0].value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'indexed_replace': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder();
 Holder original = holders[0]; holders[0].value = SharedPointers.MAKE<Value>(); int result = 0;
 match (holders[0].value.Get()) { ref item => { holders[0] = new Holder(); result = (int)item.Data; } }
 delete original; delete holders[0]; delete[] holders; return result; } }''', None),
    'indexed_delete_element': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete holders[0]; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_alias': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder(); Holder alias = holders[0];
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_forwarded_alias': ('''class Program {
 static Holder Identity(Holder value) { return value; }
 public static int Main() { Holder[] holders = new Holder[1]; holders[0] = new Holder();
 Holder alias = Identity(holders[0]); holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_reassigned_call': ('''class Program {
 static Holder Identity(Holder value) { return value; }
 public static int Main() { Holder[] holders = new Holder[1]; holders[0] = new Holder();
 Holder alias = null; alias = Identity(holders[0]); holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_static_alias': ('''class Registry { public static Holder Saved; }
class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder(); Registry.Saved = holders[0];
 Holder alias = Registry.Saved; holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_static_call': ('''class Registry { public static Holder Saved; public static Holder Read() { return Saved; } }
class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder(); Registry.Saved = holders[0];
 Holder alias = Registry.Read(); holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_parameter': ('''class Program {
 static int Work(Holder parameter) { Holder[] holders = new Holder[1]; holders[0] = parameter;
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_delete_parameter_array_parameter': ('''class Program {
 static int Work(Holder[] holders, Holder parameter) {
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_delete_parameter_array_other_array': ('''class Program {
 static int Work(Holder[] holders, Holder[] other) {
 match (holders[0].value.Get()) { ref item => { delete other[0]; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_parameter_array_overwrite': ('''class Program {
 static int Work(Holder[] holders, Holder[] other) {
 match (holders[0].value.Get()) { ref item => {
 other[0].value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'indexed_parameter_array_call': ('''class Program {
 static int Work(Holder[] holders, Holder[] other) {
 match (holders[0].value.Get()) { ref item => { other[0].Reset(); return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'cannot call potentially mutating method while a borrowed field view is alive'),
    'indexed_parameter_array_delete_buffer': ('''class Program {
 static int Work(Holder[] holders) {
 match (holders[0].value.Get()) { ref item => { delete[] holders; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed'),
    'indexed_parameter_array_delete_buffer_alias': ('''class Program {
 static int Work(Holder[] holders) { Holder[] alias = holders;
 match (holders[0].value.Get()) { ref item => { delete[] alias; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed'),
    'indexed_parameter_array_delete_static_buffer': ('''class Registry { public static Holder[] Saved; }
 class Program { static int Work(Holder[] holders) {
 match (holders[0].value.Get()) { ref item => { delete[] Registry.Saved; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed'),
    'indexed_parameter_array_delete_called_buffer': ('''class Registry { public static Holder[] Saved;
 public static Holder[] Read() { return Saved; } }
 class Program { static int Work(Holder[] holders) {
 match (holders[0].value.Get()) { ref item => { delete[] Registry.Read(); return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed'),
    'indexed_parameter_array_delete_independent_buffer': ('''class Program {
 static int Work(Holder[] holders) { Holder[] independent = new Holder[1];
 match (holders[0].value.Get()) { ref item => { delete[] independent; return (int)item.Data; } }
 return 0; } public static int Main() { Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Value>(); int result = Work(holders);
 delete holders[0]; delete[] holders; return result; } }''', None),
    'indexed_read_parameter_array': ('''class Program {
 static int Work(Holder[] holders) {
 match (holders[0].value.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 0; } public static int Main() { Holder[] holders = new Holder[1];
 holders[0] = new Holder(); holders[0].value = SharedPointers.MAKE<Value>();
 int result = Work(holders); delete holders[0]; delete[] holders; return result; } }''', None),
    'indexed_delete_initializer_parameter': ('''class Program {
 static int Work(Holder parameter) { Holder[] holders = new Holder[] { parameter };
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_delete_reassigned_initializer_parameter': ('''class Program {
 static int Work(Holder parameter) { Holder[] holders = new Holder[1];
 holders = new Holder[] { parameter }; holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_delete_reassigned_spread_alias': ('''class Program { public static int Main() {
 Holder[] first = new Holder[1]; first[0] = new Holder(); Holder[] second = new Holder[1];
 second = [..first]; first[0].value = SharedPointers.MAKE<Value>();
 match (first[0].value.Get()) { ref item => { delete second[0]; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_reassigned_spread_independent': ('''class Program { public static int Main() {
 Holder[] first = new Holder[1]; first[0] = new Holder(); Holder[] independent = new Holder[1];
 independent[0] = new Holder(); Holder[] second = new Holder[1]; second = [..independent];
 first[0].value = SharedPointers.MAKE<Value>();
 match (first[0].value.Get()) { ref item => { delete second[0]; return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'indexed_delete_initializer_other_array': ('''class Program {
 public static int Main() { Holder[] first = new Holder[1]; first[0] = new Holder();
 Holder[] second = new Holder[] { first[0] }; first[0].value = SharedPointers.MAKE<Value>();
 match (first[0].value.Get()) { ref item => { delete second[0]; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_initializer_independent': ('''class Program {
 public static int Main() { Holder[] first = new Holder[] { new Holder() };
 Holder[] second = new Holder[] { new Holder() }; first[0].value = SharedPointers.MAKE<Value>();
 match (first[0].value.Get()) { ref item => { delete second[0]; return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'indexed_delete_collection_parameter': ('''class Program {
 static int Work(Holder parameter) { Holder[] holders = [parameter];
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_delete_collection_spread': ('''class Program {
 public static int Main() { Holder[] first = new Holder[1]; first[0] = new Holder();
 Holder[] second = [..first]; first[0].value = SharedPointers.MAKE<Value>();
 match (first[0].value.Get()) { ref item => { delete second[0]; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_collection_independent_spread': ('''class Program {
 public static int Main() { Holder[] first = new Holder[1]; first[0] = new Holder();
 Holder[] other = new Holder[1]; other[0] = new Holder(); Holder[] second = [..other];
 first[0].value = SharedPointers.MAKE<Value>();
 match (first[0].value.Get()) { ref item => { delete second[0]; return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'indexed_delete_parameter_via_array_alias': ('''class Program {
 static int Work(Holder parameter) { Holder[] holders = new Holder[1]; Holder[] alias = holders;
 alias[0] = parameter; holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_delete_shared_object_via_other_array': ('''class Program {
 public static int Main() { Holder[] first = new Holder[1]; first[0] = new Holder();
 Holder[] second = new Holder[1]; second[0] = first[0];
 first[0].value = SharedPointers.MAKE<Value>();
 match (first[0].value.Get()) { ref item => { delete second[0]; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_delete_parameter_after_fill': ('''class Program {
 static void Fill(Holder[] array, Holder value) { array[0] = value; }
 static int Work(Holder parameter) { Holder[] holders = new Holder[1]; Fill(holders, parameter);
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_delete_parameter_after_static_fill': ('''class Registry { public static Holder[] Saved;
 public static void Fill(Holder value) { Saved[0] = value; } }
 class Program { static int Work(Holder parameter) { Holder[] holders = new Holder[1];
 Registry.Saved = holders; Registry.Fill(parameter); holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_delete_parameter_after_object_fill': ('''class Box { public Holder[] Saved;
 public void Fill(Holder value) { Saved[0] = value; } }
 class Program { static int Work(Holder parameter) { Holder[] holders = new Holder[1]; Box box = new Box();
 box.Saved = holders; box.Fill(parameter); holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete parameter; return (int)item.Data; } }
 return 0; } public static int Main() { return 42; } }''', 'while a borrowed view is alive'),
    'indexed_destroy_alias': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder(); Holder alias = holders[0];
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { destroy alias; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_replace_then_delete_original': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder(); Holder original = holders[0];
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => {
  holders[0] = new Holder(); delete original; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed view is alive'),
    'indexed_owned_array_delete': ('''class Program { public static int Main() {
 owned Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { delete[] holders; return (int)item.Data; } }
 return 0; } }''', 'while a borrowed local is alive'),
    'indexed_alias_overwrite': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder(); Holder alias = holders[0];
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { alias.value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'indexed_alias_call': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder(); Holder alias = holders[0];
 holders[0].value = SharedPointers.MAKE<Value>();
 match (holders[0].value.Get()) { ref item => { alias.Reset(); return (int)item.Data; } }
 return 0; } }''', 'cannot call potentially mutating method while a borrowed field view is alive'),
    'indexed_other_field': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Value>(); int result = 0;
 match (holders[0].value.Get()) { ref item => {
  holders[0].other = SharedPointers.MAKE<Value>(); result = (int)item.Data; } }
 delete holders[0]; delete[] holders; return result; } }''', None),
    'indexed_independent_delete': ('''class Program { public static int Main() {
 Holder[] holders = new Holder[1]; holders[0] = new Holder();
 Holder[] independent = new Holder[1]; independent[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Value>(); int result = 0;
 match (holders[0].value.Get()) { ref item => { delete independent[0]; result = (int)item.Data; } }
 delete[] independent; delete holders[0]; delete[] holders; return result; } }''', None),
    'nested_other': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { outer.inner.other = SharedPointers.MAKE<Value>();
 return item.Data == 42l ? 42 : 1; } } return 0; } }''', None),
    'nested_overwrite': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { outer.inner.value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'nested_move': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { FAMILY<Value> moved = move(outer.inner.value); return (int)item.Data; } }
 return 0; } }''', 'cannot partially move owner field while a borrowed view is alive'),
    'nested_alias_overwrite': ('''class Program { public static int Main() {
 Outer outer = new Outer(); Holder alias = outer.inner; outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { alias.value = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'nested_parent_replace': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { outer.inner = new Holder(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'nested_call': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { outer.inner.Reset(); return (int)item.Data; } }
 return 0; } }''', 'cannot call potentially mutating method while a borrowed field view is alive'),
    'nested_finished': ('''class Program { public static int Main() {
 Outer outer = new Outer(); outer.inner.value = SharedPointers.MAKE<Value>();
 match (outer.inner.value.Get()) { ref item => { if (item.Data != 42l) return 1; } }
 outer.inner = new Holder(); outer.inner.value = SharedPointers.MAKE<Value>();
 return outer.inner.value.Get().Data == 42l ? 42 : 2; } }''', None),
    'deep_read': ('''class Program { public static int Main() {
 Deep root = new Deep(); root.outer.inner.value = SharedPointers.MAKE<Value>();
 match (root.outer.inner.value.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 0; } }''', None),
    'deep_parent_replace': ('''class Program { public static int Main() {
 Deep root = new Deep(); root.outer.inner.value = SharedPointers.MAKE<Value>();
 match (root.outer.inner.value.Get()) { ref item => { root.outer = new Outer(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'deep_middle_replace': ('''class Program { public static int Main() {
 Deep root = new Deep(); root.outer.inner.value = SharedPointers.MAKE<Value>();
 match (root.outer.inner.value.Get()) { ref item => { root.outer.inner = new Holder(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner field while a borrowed view is alive'),
    'finished': ('''class Program { public static int Main() {
 Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { if (item.Data != 42l) return 1; } }
 holder.value = SharedPointers.MAKE<Value>(); return holder.value.Get().Data == 42l ? 42 : 2; } }''', None),
}


def run(command, cwd):
    result = subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True,
                            text=True, errors='replace', timeout=120)
    print('COMMAND', subprocess.list2cmdline([str(x) for x in command]), 'EXIT', result.returncode,
          result.stdout + result.stderr, flush=True)
    return result


def main():
    compiler = Path(sys.argv[1]).resolve()
    library = Path(sys.argv[2]).resolve()
    selected = os.environ.get('NSC_BINARY_PATTERN_CASES')
    cases = CASES
    if selected:
        requested = set(selected.split(','))
        unknown = requested.difference(CASES)
        if unknown:
            raise ValueError('unknown binary pattern cases: ' + ', '.join(sorted(unknown)))
        cases = {name: case for name, case in CASES.items() if name in requested}
    failures = []
    with tempfile.TemporaryDirectory(prefix='nsc-binary-field-pattern-') as temporary:
        root = Path(temporary)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>BinaryPattern</Name></PropertyGroup>'
                           '<ItemGroup><Compile Include="Main.ns"/><LibraryReference Include="' +
                           library.as_posix() + '" Metadata="' + library.with_suffix('.nsmeta').as_posix() +
                           '"/></ItemGroup></Project>', encoding='utf-8')
        for family, make in (('Rc', 'MakeRc'), ('Arc', 'MakeArc')):
            for name, (body, diagnostic) in cases.items():
                if family == 'Rc' and name in ('indexed_delete_static_alias', 'indexed_delete_static_call',
                                                'indexed_delete_parameter_after_static_fill',
                                                'nested_direct_delete_static_alias',
                                                'nested_direct_delete_static_field',
                                                'nested_direct_destroy_static_field',
                                                'nested_direct_delete_static_field_finished',
                                                'nested_chained_view_delete_static',
                                                'nested_chained_view_delete_static_reassigned',
                                                'nested_chained_view_delete_static_call',
                                                'nested_direct_delete_static_forwarded',
                                                'nested_direct_delete_static_with_argument',
                                                'nested_direct_delete_static_call_expression',
                                                'nested_direct_destroy_static_call_expression',
                                                'nested_direct_destroy_static_alias',
                                                'nested_direct_delete_static_reassigned',
                                                'nested_direct_destroy_static_reassigned_alias',
                                                'nested_direct_delete_static_call_finished'):
                    print('SKIP', family, name, 'static storage of Rc is forbidden', flush=True)
                    continue
                source = (PREFIX + body).replace('FAMILY', family).replace('MAKE', make)
                (root / 'Main.ns').write_text(source, encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                if family == 'Rc' and name in ('indexed_parameter_array_delete_static_buffer',
                                                'indexed_parameter_array_delete_called_buffer'):
                    diagnostic = 'shared static storage cannot contain a thread-local shared pointer'
                if diagnostic is not None:
                    if result.returncode == 0 or diagnostic not in output:
                        failures.append((family, name, 'unexpected diagnostic'))
                    continue
                if result.returncode:
                    failures.append((family, name, 'check'))
                    continue
                for level in ('O0', 'O2'):
                    executable = root / (family + '-' + name + '-' + level + '.exe')
                    if run([compiler, '--emit-project-oop-exe', project, '-' + level, '-o', executable,
                            '--no-artifact-cache'], root).returncode:
                        failures.append((family, name, level, 'compile'))
                        continue
                    result = run([executable], root)
                    if result.returncode != 42:
                        failures.append((family, name, level, 'execute', result.returncode))
    if failures:
        print('FAIL', failures, file=sys.stderr, flush=True)
        return 1
    print('shared_binary_pattern_field: PASS', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
