import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd):
    return subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True,
                          text=True, errors='replace', timeout=120)


def main():
    compiler, *sources = map(Path, sys.argv[1:])
    prefix = '''using NativeSharp.Core;
namespace NativeSharp.Core { public class Exception { } }
class Payload { public static int Drops; public long Data; public Payload() { Data = 42l; }
 ~Payload() { Drops = Drops + 1; } }
struct Value { public static int Drops; public long Data; public Value() { Data = 42l; }
 ~Value() { Drops = Drops + 1; } }
class Resource { public static int Drops; ~Resource() { Drops = Drops + 1; } }
class ArrayPayload { public int[] Data; public ArrayPayload() { Data = new int[1]; } }
class Child { public int Value; }
struct ChildValue { public Child Item; public int Number;
 public ChildValue() { Item = new Child(); Number = 42; } }
class ChildArrayPayload { public Child[] Data; public ChildArrayPayload() { Data = new Child[1]; Data[0] = new Child(); } }
class Bad { public owned Resource item;
 public Bad() throws Exception { item = new Resource(); throw new Exception(); } }
struct BadValue { public owned Resource item;
 public BadValue() throws Exception { item = new Resource(); throw new Exception(); } }
'''
    safe = '''class Program { public static int Main() {
 WEAK<Payload> weak;
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); weak = owner.Downgrade();
   if (owner.IsEmpty() || owner.StrongCount() != 1l || weak.Expired() || weak.WeakCount() != 1l) return 1;
   { WEAK<Payload> weak2 = weak.Clone(); STRONG<Payload> clone = weak2.Upgrade();
     SharedView<Payload> view = owner.Borrow(); borrowed Payload alias = owner.Get();
     if (view.Get().Data != 42l || alias.Data != 42l || clone.Get().Data != 42l ||
         owner.StrongCount() != 2l || weak.WeakCount() != 2l) return 2;
   }
   if (owner.StrongCount() != 1l || weak.WeakCount() != 1l || Payload.Drops != 0) return 3;
 }
 if (!weak.Expired() || Payload.Drops != 1) return 4;
 { STRONG<Payload> dead = weak.Upgrade(); if (!dead.IsEmpty()) return 5; }
 { STRONG<Value> owner = SharedPointers.MAKE<Value>(); WEAK<Value> valueWeak = owner.Downgrade();
   if (owner.Get().Data != 42l || owner.Borrow().Get().Data != 42l || valueWeak.Expired()) return 6;
 }
 if (Value.Drops != 1) return 7;
 int caught = 0;
 try { STRONG<Bad> failed = SharedPointers.MAKE_THROWING<Bad>(); }
 catch (Exception error) { caught = caught + 1; }
 try { STRONG<BadValue> failed = SharedPointers.MAKE_THROWING<BadValue>(); }
 catch (Exception error) { caught = caught + 1; }
 return caught == 2 && Resource.Drops == 2 ? 42 : 8;
} }'''
    cases = {
        'safe': (safe, None),
        'struct_pattern_ref_read': ('''class Program { public static int Work(STRONG<Value> owner) {
 match (owner.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 2; } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''', 'run:42'),
        'struct_pattern_ref_local_control': ('''class Program { public static int Main() {
 Value value = default(Value); value.Data = 42l;
 match (value) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 2; } }''', 'run:42'),
        'struct_pattern_ref_write': ('''class Program { public static int Work(STRONG<Value> owner) {
 match (owner.Get()) { ref item => { item.Data = 9l; } } return 0;
 } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''', 'not assignable'),
        'struct_pattern_ref_overwrite': ('''class Program { public static int Work(STRONG<Value> owner) {
 match (owner.Get()) { ref item => { owner = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''',
                                         'cannot overwrite owner'),
        'struct_pattern_ref_move': ('''class Program { public static int Work(STRONG<Value> owner) {
 match (owner.Get()) { ref item => { STRONG<Value> other = move(owner); return (int)item.Data; } }
 return 0; } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''', 'cannot move'),
        'struct_pattern_local_overwrite': ('''class Program { public static int Main() {
 STRONG<Value> owner = SharedPointers.MAKE<Value>();
 match (owner.Get()) { ref item => { owner = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } }''', 'cannot overwrite owner'),
        'struct_pattern_local_move': ('''class Program { public static int Main() {
 STRONG<Value> owner = SharedPointers.MAKE<Value>();
 match (owner.Get()) { ref item => { STRONG<Value> other = move(owner); return (int)item.Data; } }
 return 0; } }''', 'cannot move'),
        'struct_pattern_child_return': ('''class Program {
 public static borrowed Child Leak(STRONG<ChildValue> owner) {
 match (owner.Get()) { ref item => { return item.Item; } } return null;
 } public static int Main() { return 0; } }''', 'cannot be returned'),
        'struct_pattern_finished': ('''class Program { public static int Work(STRONG<Value> owner) {
 match (owner.Get()) { ref item => { if (item.Data != 42l) return 1; } }
 owner = SharedPointers.MAKE<Value>(); return owner.Get().Data == 42l ? 42 : 2;
 } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''', 'run:42'),
        'struct_pattern_temporary': ('''class Program { public static int Main() {
 match (SharedPointers.MAKE<Value>().Get()) { ref item => { return (int)item.Data; } }
 return 0; } }''', 'borrowed local requires'),
        'struct_pattern_view_read': ('''class Program { public static int Work(STRONG<Value> owner) {
 SharedView<Value> view = owner.Borrow();
 match (view.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 2; } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''', 'run:42'),
        'struct_pattern_chained_view_read': ('''class Program { public static int Work(STRONG<Value> owner) {
 match (owner.Borrow().Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 2; } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''', 'run:42'),
        'struct_pattern_field_read': ('''class Holder { public STRONG<Value> value; }
 class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Value>();
 match (holder.value.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 2; } }''', 'run:42'),
        'struct_pattern_view_overwrite': ('''class Program { public static int Work(STRONG<Value> owner) {
 SharedView<Value> view = owner.Borrow();
 match (view.Get()) { ref item => { owner = SharedPointers.MAKE<Value>(); return (int)item.Data; } }
 return 0; } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''',
                                          'cannot overwrite owner'),
        'struct_pattern_child_write': ('''class Program { public static int Work(STRONG<ChildValue> owner) {
 match (owner.Get()) { ref item => { item.Item.Value = 9; } }
 return 0; } public static int Main() { return Work(SharedPointers.MAKE<ChildValue>()); } }''',
                                       'not assignable'),
        'struct_pattern_child_escape': ('''class Program { public static int Work(STRONG<ChildValue> owner) {
 match (owner.Get()) { ref item => { Child alias = item.Item; return alias.Value; } }
 return 0; } public static int Main() { return Work(SharedPointers.MAKE<ChildValue>()); } }''',
                                        'borrowed value cannot be stored'),
        'pattern_ordinary_receiver_control': ('''class Box { public int Value;
 public ref[this] int Get() { return ref Value; } }
 class Program { public static int Main() { Box box = new Box(); box.Value = 42;
 match (box.Get()) { ref item => { return item; } } return 1; } }''', 'run:42'),
        'pattern_mixed_mutable_control': ('''struct PairValue { public int First; public int Second; }
 class Program { public static int Main() { PairValue value = default(PairValue); value.First = 40;
 match (value) { { First: ref a, Second: ref mut b } => { b = a + 2; }, _ => { return 1; } }
 return value.Second == 42 ? 42 : 2; } }''', 'run:42'),
        'struct_pattern_mut_readonly': ('''class Program { public static int Work(STRONG<Value> owner) {
 match (owner.Get()) { ref mut item => { item.Data = 9l; } } return 0;
 } public static int Main() { return Work(SharedPointers.MAKE<Value>()); } }''',
                                        'mutable addressable storage'),
        'struct_pattern_mut_local_control': ('''class Program { public static int Main() {
 Value value = default(Value); match (value) { ref mut item => { item.Data = 42l; } }
 return value.Data == 42l ? 42 : 1; } }''', 'run:42'),
        'class_pattern_read': ('''class Program { public static int Work(STRONG<Payload> owner) {
 match (owner.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 2; } public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'run:42'),
        'class_pattern_write': ('''class Program { public static int Work(STRONG<Payload> owner) {
 match (owner.Get()) { ref item => { item.Data = 9l; } } return 0;
 } public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'not assignable'),
        'class_pattern_escape': ('''class Program { public static borrowed Payload Leak(STRONG<Payload> owner) {
 match (owner.Get()) { ref item => { return item; } } return null;
 } public static int Main() { return 0; } }''', 'cannot be returned'),
        'class_pattern_overwrite': ('''class Program { public static int Work(STRONG<Payload> owner) {
 match (owner.Get()) { ref item => { owner = SharedPointers.MAKE<Payload>(); return (int)item.Data; } }
 return 0; } public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''',
                                    'cannot overwrite owner'),
        'class_pattern_view_read': ('''class Program { public static int Work(STRONG<Payload> owner) {
 SharedView<Payload> view = owner.Borrow();
 match (view.Get()) { ref item => { return item.Data == 42l ? 42 : 1; } }
 return 2; } public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'run:42'),
        'struct_pattern_view_rebind_owner': ('''class Program {
 public static int Work(STRONG<Value> owner, STRONG<Value> other) {
 SharedView<Value> view = owner.Borrow();
 match (view.Get()) { ref item => { view = other.Borrow(); owner = SharedPointers.MAKE<Value>();
 return (int)item.Data; } } return 0; }
 public static int Main() { return Work(SharedPointers.MAKE<Value>(), SharedPointers.MAKE<Value>()); } }''',
                                             'cannot overwrite owner'),
        'class_pattern_method_mutation': ('''class MutablePayload { public int Data;
 public void Change() { Data = 9; } }
 class Program { public static int Work(STRONG<MutablePayload> owner) {
 match (owner.Get()) { ref item => { item.Change(); } } return 0;
 } public static int Main() { return Work(SharedPointers.MAKE<MutablePayload>()); } }''',
                                           'readonly borrowed'),
        'normal_factory_base_static_throw': ('''class Base { public static int Built; public static int Drops;
 public Base() { Built = Built + 1; } ~Base() { Drops = Drops + 1; } }
 class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Derived : Base { public int Number; public Derived() { Number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); }
 catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Base.Built != 1 ? 5 : Base.Drops == 0 ? 3 : Base.Drops == 1 ? 42 : 4; } }''', 'run:42'),
        'normal_factory_base_success': ('''class Base { public static int Built; public static int Drops;
 public Base() { Built = Built + 1; } ~Base() { Drops = Drops + 1; } }
 class Derived : Base { public int Number; public Derived() { Number = 7; } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (owner.Get().Number != 7 || Base.Built != 1 || Base.Drops != 0) return 1; }
 return Base.Drops == 1 ? 42 : 2; } }''', 'run:42'),
        'normal_factory_base_incomplete': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public static int Started; public static int Drops; public int Number;
 public Base() { Started = Started + 1; Number = Gate.Read(); } ~Base() { Drops = Drops + 1; } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && Base.Started == 1 && Base.Drops == 0 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_incomplete_owned': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public static int Started; public static int Ready; public static int Drops; public owned Resource resource;
 public int Number; public Base() { Started = Started + 1; resource = new Resource();
 if (resource != null) Ready = 1; Number = Gate.Read(); }
 ~Base() { Drops = Drops + 1; } } class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); }
 catch (Exception error) { caught = 1; }
 return caught != 1 || Base.Started != 1 || Base.Ready != 1 || Base.Drops != 0 ? 2 :
 Resource.Drops == 0 ? 3 : Resource.Drops == 1 ? 42 : 4; } }''',
                                                'run:42'),
        'normal_factory_base_owned_reassign_throw': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public Base() { resource = new Resource(); resource = new Resource(); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 2 ? 42 : Resource.Drops; } }''', 'run:42'),
        'normal_factory_owned_replacement_fails': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource;
 private static owned Resource Create() { int value = Gate.Read(); return new Resource(); }
 public Base() { resource = new Resource(); resource = Create(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops == 1 ? 42 : Resource.Drops + 3; } }''', 'run:42'),
        'normal_factory_owned_replacement_recovers': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public static int Caught; public owned Resource resource;
 private static owned Resource Create() { int value = Gate.Read(); return new Resource(); }
 public Base() { resource = new Resource(); try { resource = Create(); } catch (Exception error) { Caught = 1; } } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (Base.Caught != 1 || Resource.Drops != 0) return 1; }
 return Resource.Drops == 1 ? 42 : Resource.Drops + 3; } }''', 'run:42'),
        'normal_factory_owned_replacement_failure_order': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public owned Part first; public owned Part second;
 private static owned Part Create() { int value = Gate.Read(); return new Part(3); }
 public Base() { first = new Part(1); second = new Part(2); second = Create(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 21 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_owned_replacement_conditional_failure': ('''class Gate {
 static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public owned Resource resource;
 private static owned Resource Create() { int value = Gate.Read(); return new Resource(); }
 public Base() { resource = new Resource(); bool choose = false; resource = choose ? new Resource() : Create(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops == 1 ? 42 : Resource.Drops + 3; } }''', 'requires an ownership-transfer expression'),
        'normal_factory_owned_replacement_expression_failure': ('''class Gate {
 static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public owned Resource resource;
 private static owned Resource Create() { int value = Gate.Read(); return new Resource(); }
 public Base() { resource = new Resource(); if ((resource = Create()) != null) { } } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops == 1 ? 42 : Resource.Drops + 3; } }''', 'run:42'),
        'normal_factory_base_owned_caught': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public static int Caught; public owned Resource resource; public Base() {
 resource = new Resource(); try { int value = Gate.Read(); } catch (Exception error) { Caught = 1; } } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (Base.Caught != 1 || Resource.Drops != 0) return 1; }
 return Resource.Drops == 1 ? 42 : 2; } }''', 'run:42'),
        'normal_factory_base_owned_order': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public owned Part first; public owned Part second; public int Number;
 public Base() { second = new Part(2); first = new Part(1); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 21 ? 42 : Part.Trace; } }''', 'run:42'),
        'tuple_shared_owner_partial_failure': ('''class Gate {
 static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Program { public static int Main() { int caught = 0;
 try { (STRONG<Payload>, int) pair = (SharedPointers.MAKE<Payload>(), Gate.Read()); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && Payload.Drops == 1 ? 42 : Payload.Drops + 3; } }''', 'run:42'),
        'tuple_shared_owner_drop_unstarted': ('''class Data { public static int Builds; public static int Drops;
 public Data() { Builds = Builds + 1; } ~Data() { Drops = Drops + 1; } }
 class Program { public static async int Slow() { yield; return 7; }
 public static AsyncIterator<int, int> Gen() {
 (STRONG<Data>, int) pair = (SharedPointers.MAKE<Data>(), await Slow()); yield return 7; }
 public static int Main() { AsyncIterator<int, int> iterator = Gen(); iterator.Drop();
 return Data.Builds == 0 && Data.Drops == 0 ? 42 : 1; } }''', 'run:42'),
        'tuple_shared_owner_drop_iterator': ('''class Program { public static async int Slow() { yield; return 7; }
 public static AsyncIterator<int, int> Gen() {
 (STRONG<Payload>, int) pair = (SharedPointers.MAKE<Payload>(), await Slow()); yield return 7; }
 public static async int Main() { AsyncIterator<int, int> iterator = Gen();
 bool first = await iterator.MoveNextAsync(); if (!first || Payload.Drops != 0) return 1;
 iterator.Drop(); return Payload.Drops == 1 ? 42 : Payload.Drops + 3; } }''', 'run:42'),
        'tuple_shared_owner_cancel': ('''class Program { public static async int Slow() { yield; return 7; }
 public static AsyncIterator<int, int> Gen() {
 (STRONG<Payload>, int) pair = (SharedPointers.MAKE<Payload>(), await Slow()); yield return 7; }
 public static async int Main() { AsyncIterator<int, int> iterator = Gen();
 bool first = await iterator.MoveNextAsync(); if (!first || Payload.Drops != 0) return 1;
 iterator.Cancel(); if (Payload.Drops != 0) return 2;
 bool second = await iterator.MoveNextAsync();
 return !second && Payload.Drops == 1 ? 42 : Payload.Drops + 3; } }''', 'run:42'),
        'tuple_shared_owner_await_success': ('''class Data { public static int Builds; public static int Drops;
 public Data() { Builds = Builds + 1; } ~Data() { Drops = Drops + 1; } }
 class Program { public static async int Slow() { yield; return 7; }
 public static async int Main() {
 { (STRONG<Data>, int) pair = (SharedPointers.MAKE<Data>(), await Slow());
 if (Data.Builds != 1 || Data.Drops != 0) return 1; }
 return Data.Builds == 1 && Data.Drops == 1 ? 42 : 2; } }''', 'run:42'),
        'tuple_shared_owner_await_failure': ('''class Program {
 public static async int SlowFail() throws Exception { yield; throw new Exception(); }
 public static async int Main() { int caught = 0;
 try { (STRONG<Payload>, int) pair = (SharedPointers.MAKE<Payload>(), await SlowFail()); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && Payload.Drops == 1 ? 42 : Payload.Drops + 3; } }''', 'run:42'),
        'tuple_shared_owner_nested_failure': ('''class Data { public static int Count; public static int Trace;
 public int Id; public Data() { Count = Count + 1; Id = Count; } ~Data() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Program { public static int Main() { int caught = 0;
 try { (STRONG<Data>, (STRONG<Data>, int)) pair =
   (SharedPointers.MAKE<Data>(), (SharedPointers.MAKE<Data>(), Gate.Read())); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && Data.Trace == 21 ? 42 : Data.Trace; } }''', 'run:42'),
        'tuple_shared_owner_success': ('''class Program { public static int Main() {
 { (STRONG<Payload>, int) pair = (SharedPointers.MAKE<Payload>(), 7);
 if (Payload.Drops != 0) return 1; }
 return Payload.Drops == 1 ? 42 : Payload.Drops + 3; } }''', 'run:42'),
        'normal_factory_delegating_inner_failure': ('''class Gate {
 static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Box { public static int BoxDrops; public owned Resource item; public int Number;
 public Box() : this(1) { } private Box(int marker) { item = new Resource(); Number = Gate.Read(); }
 ~Box() { BoxDrops = BoxDrops + 1; } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Box> owner = SharedPointers.MAKE<Box>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Box.BoxDrops != 0 ? 3 : Resource.Drops == 1 ? 42 : Resource.Drops + 4; } }''', 'run:42'),
        'normal_factory_delegating_outer_failure': ('''class Gate {
 static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Box { public owned Resource item; public int Number;
 public Box() : this(1) { Number = Gate.Read(); } private Box(int marker) { item = new Resource(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Box> owner = SharedPointers.MAKE<Box>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops == 1 ? 42 : Resource.Drops + 4; } }''', 'run:42'),
        'normal_factory_delegating_caught': ('''class Gate {
 static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Box { public static int BoxDrops; public static int Caught; public owned Resource item;
 public Box() : this(1) { try { int n = Gate.Read(); } catch (Exception error) { Caught = 1; } }
 private Box(int marker) { item = new Resource(); } ~Box() { BoxDrops = BoxDrops + 1; } }
 class Program { public static int Main() { { STRONG<Box> owner = SharedPointers.MAKE<Box>();
 if (Box.Caught != 1 || Box.BoxDrops != 0 || Resource.Drops != 0) return 2; }
 return Box.BoxDrops == 1 && Resource.Drops == 1 ? 42 : 3; } }''', 'run:42'),
        'normal_factory_delegating_success': ('''class Box { public static int BoxDrops; public owned Resource item;
 public Box() : this(1) { } private Box(int marker) { item = new Resource(); }
 ~Box() { BoxDrops = BoxDrops + 1; } }
 class Program { public static int Main() { { STRONG<Box> owner = SharedPointers.MAKE<Box>();
 if (Box.BoxDrops != 0 || Resource.Drops != 0) return 2; }
 return Box.BoxDrops == 1 && Resource.Drops == 1 ? 42 : 3; } }''', 'run:42'),
        'normal_factory_partial_owned_order': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 partial class Normal { public owned Part first; }
 partial class Normal { public owned Part second; public Normal() { second = new Part(2); first = new Part(1); } }
 partial class Base { public owned Part first; }
 partial class Base { public owned Part second; public int Number;
 public Base() { second = new Part(2); first = new Part(1); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { { owned Normal control = new Normal(); }
 int expected = Part.Trace; Part.Trace = 0; int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && expected != 0 && Part.Trace == expected ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_partial_files_owned_order': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 partial class Normal { public owned Part second; public Normal() { second = new Part(2); first = new Part(1); } }
 partial class Base { public owned Part second; public int Number;
 public Base() { second = new Part(2); first = new Part(1); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { { owned Normal control = new Normal(); }
 int expected = Part.Trace; Part.Trace = 0; int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && expected != 0 && Part.Trace == expected ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_base_owned_local_order': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public owned Part first; public owned Part second; public int Number;
 public Base() { second = new Part(2); Part local = new Part(3); first = new Part(1); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 321 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_base_owned_helper': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public void Initialize() { resource = new Resource(); }
 public Base() { Initialize(); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : Resource.Drops + 10 * caught; } }''', 'run:42'),
        'normal_factory_base_owned_helper_throw': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public void Initialize() { resource = new Resource(); Number = Gate.Read(); }
 public Base() { Initialize(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : Resource.Drops + 10 * caught; } }''', 'run:42'),
        'normal_factory_base_owned_helper_success': ('''class Base { public owned Resource resource;
 public void Initialize() { resource = new Resource(); } public Base() { Initialize(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() {
 { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); if (Resource.Drops != 0) return 1; }
 return Resource.Drops == 1 ? 42 : 2; } }''', 'run:42'),
        'normal_factory_base_owned_helper_transfer': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public void Transfer() { owned Resource taken = move(resource); }
 public Base() { resource = new Resource(); Transfer(); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : Resource.Drops + 10 * caught; } }''', 'run:42'),
        'normal_factory_base_owned_helper_replace': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public void Replace() { resource = new Resource(); }
 public Base() { resource = new Resource(); Replace(); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 2 ? 42 : Resource.Drops + 10 * caught; } }''', 'run:42'),
        'normal_factory_base_owned_branch': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public Base() { if (true) { resource = new Resource(); } Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_owned_expression': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public Base() { if ((resource = new Resource()) != null) { Number = 1; } Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_owned_transfer': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public Base() { resource = new Resource(); owned Resource taken = move(resource); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_owned_clear': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public Base() { resource = new Resource(); resource = null; Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_owned_parameter': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public int Number; public Base(owned Resource resource) { Number = Gate.Read(); } }
 class Derived : Base { public Derived() : base(new Resource()) { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_owned_parameter_field_order': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public owned Part field; public int Number;
 public Base(owned Part parameter) { field = new Part(2); Number = Gate.Read(); } }
 class Derived : Base { public Derived() : base(new Part(1)) { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 21 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_base_owned_parameter_to_field': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource field; public int Number;
 public Base(owned Resource parameter) { field = move(parameter); Number = Gate.Read(); } }
 class Derived : Base { public Derived() : base(new Resource()) { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_value_field_failure': ('''struct Part { public static int Trace; public static int Built; public int Id;
 public Part(int id) { Id = id; Built = Built + 1; } ~Part() { if (Id != 0) { Trace = Trace * 10 + Id; } } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public Part field = new Part(1); public int Number;
 public Base() { Number = Gate.Read(); } ~Base() { Part.Trace = Part.Trace * 10 + 2; } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 3 : Part.Built != 1 ? 4 : Part.Trace == 1 ? 42 : Part.Trace == 0 ? 5 : 6; } }''', 'run:42'),
        'normal_factory_base_value_field_before': ('''struct Part { public static int Trace; public static int Built; public int Id;
 public Part(int id) { Id = id; Built = Built + 1; } ~Part() { if (Id != 0) { Trace = Trace * 10 + Id; } } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public int Number = Gate.Read(); public Part field = new Part(1); public Base() { } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Built == 0 && Part.Trace == 0 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_value_field_success': ('''struct Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { if (Id != 0) { Trace = Trace * 10 + Id; } } }
 class Base { public Part field = new Part(1); public Base() { } ~Base() { Part.Trace = Part.Trace * 10 + 2; } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (Part.Trace != 0) return 1; } return Part.Trace == 21 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_base_value_default_success': ('''struct Part { public static int Drops; ~Part() { Drops = Drops + 1; } }
 class Base { public Part field; public Base() { } } class Derived : Base { public Derived() { } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (Part.Drops != 0) return 1; } return Part.Drops == 1 ? 42 : Part.Drops + 2; } }''', 'run:42'),
        'normal_factory_base_value_default_failure': ('''struct Part { public static int Drops; ~Part() { Drops = Drops + 1; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public Part field; public int Number; public Base() { Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } } class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Part.Drops == 1 ? 42 : Part.Drops + 3; } }''', 'run:42'),
        'normal_factory_base_value_partial_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 struct Part { public static int Drops; public owned Resource item; public int Number;
 public Part(int id) { item = new Resource(); Number = Gate.Read(); } ~Part() { Drops = Drops + 1; } }
 class Base { public Part field = new Part(1); public Base() { } } class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops != 1 ? 3 : Part.Drops != 0 ? 4 : 42; } }''', 'run:42'),
        'normal_factory_base_value_helper_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 struct Part { public static int Drops; public owned Resource item; public int Number;
 public void Initialize() { item = new Resource(); }
 public Part(int id) { Initialize(); Number = Gate.Read(); } ~Part() { Drops = Drops + 1; } }
 class Base { public Part field = new Part(1); public Base() { } } class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops != 1 ? 3 : Part.Drops != 0 ? 4 : 42; } }''', 'run:42'),
        'normal_factory_base_value_helper_throw': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 struct Part { public static int Drops; public owned Resource item; public int Number;
 public void Initialize() { item = new Resource(); Number = Gate.Read(); }
 public Part(int id) { Initialize(); } ~Part() { Drops = Drops + 1; } }
 class Base { public Part field = new Part(1); public Base() { } } class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops != 1 ? 3 : Part.Drops != 0 ? 4 : 42; } }''', 'run:42'),
        'normal_factory_base_value_helper_success': ('''struct Part { public static int Drops; public owned Resource item;
 public void Initialize() { item = new Resource(); }
 public Part(int id) { Initialize(); } ~Part() { Drops = Drops + 1; } }
 class Base { public Part field = new Part(1); public Base() { } } class Derived : Base { public Derived() { } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (Resource.Drops != 0 || Part.Drops != 0) return 1; }
 return Resource.Drops == 1 && Part.Drops == 1 ? 42 : 2; } }''', 'run:42'),
        'value_payload_helper_implicit_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 struct Part { public static int Drops; public owned Resource item; public int Number;
 public void Initialize() { item = new Resource(); } public Part() { Initialize(); Number = Gate.Read(); }
 ~Part() { Drops = Drops + 1; } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Part> owner = SharedPointers.MAKE<Part>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops != 1 ? 3 : Part.Drops != 0 ? 4 : 42; } }''', 'run:42'),
        'value_payload_helper_explicit_failure': ('''struct Part { public static int Drops; public owned Resource item;
 public void Initialize() { item = new Resource(); }
 public Part() throws Exception { Initialize(); throw new Exception(); }
 ~Part() { Drops = Drops + 1; } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Part> owner = SharedPointers.MAKE_THROWING<Part>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops != 1 ? 3 : Part.Drops != 0 ? 4 : 42; } }''', 'run:42'),
        'value_payload_helper_success': ('''struct Part { public static int Drops; public owned Resource item;
 public void Initialize() { item = new Resource(); } public Part() { Initialize(); }
 ~Part() { Drops = Drops + 1; } }
 class Program { public static int Main() { { STRONG<Part> owner = SharedPointers.MAKE<Part>();
 if (Resource.Drops != 0 || Part.Drops != 0) return 1; }
 return Resource.Drops == 1 && Part.Drops == 1 ? 42 : 2; } }''', 'run:42'),
        'normal_factory_base_value_partial_order': ('''class Trace { public static int Value; }
 class Item { public int Id; public Item(int id) { Id = id; } ~Item() { Trace.Value = Trace.Value * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 struct Part { public owned Item item; public int Number;
 public Part(int id) { item = new Item(2); Number = Gate.Read(); } ~Part() { Trace.Value = Trace.Value * 10 + 9; } }
 class Base { public owned Item first = new Item(1); public Part second = new Part(0); public Base() { } }
 class Derived : Base { public Derived() { } } class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Trace.Value == 21 ? 42 : Trace.Value; } }''', 'run:42'),
        'normal_factory_base_value_field_move': ('''struct Part { public static int Drops; public int Id;
 public Part(int id) { Id = id; } ~Part() { Drops = Drops + 1; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public Part field = new Part(1); public int Number;
 public Base() { Part taken = move(field); Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } } class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Part.Drops == 1 ? 42 : Part.Drops + 3; } }''', 'run:42'),
        'normal_factory_base_value_field_move_replace': ('''struct Part { public static int Drops; public int Id;
 public Part(int id) { Id = id; } ~Part() { Drops = Drops + 1; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public static int AfterReplace; public Part field = new Part(1); public int Number;
 public Base() { Part taken = move(field); field = new Part(2); AfterReplace = Part.Drops; Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } } class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Base.AfterReplace != 0 ? 4 : Part.Drops == 2 ? 42 : Part.Drops + 3; } }''', 'run:42'),
        'normal_factory_base_value_live_replace_failure': ('''struct Part { public static int Drops; public int Id;
 public Part(int id) { Id = id; } ~Part() { Drops = Drops + 1; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public static int AfterReplace; public Part field = new Part(1); public int Number;
 public Base() { field = new Part(2); AfterReplace = Part.Drops; Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } } class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Base.AfterReplace != 1 ? 4 : Part.Drops == 2 ? 42 : Part.Drops + 5; } }''', 'run:42'),
        'normal_factory_base_value_live_replace_success': ('''struct Part { public static int Drops; public int Id;
 public Part(int id) { Id = id; } ~Part() { Drops = Drops + 1; } }
 class Base { public static int AfterReplace; public Part field = new Part(1);
 public Base() { field = new Part(2); AfterReplace = Part.Drops; } }
 class Derived : Base { public Derived() { } } class Program { public static int Main() {
 { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); if (Base.AfterReplace != 1 || Part.Drops != 1) return 1; }
 return Part.Drops == 2 ? 42 : Part.Drops + 5; } }''', 'run:42'),
        'value_field_replace_in_method': ('''struct Part { public static int Drops; public int Id;
 public Part(int id) { Id = id; } ~Part() { Drops = Drops + 1; } }
 class Box { public Part field = new Part(1); public Box() { }
 public void Replace() { field = new Part(2); } }
 class Program { public static int Main() {
 { owned Box box = new Box(); box.Replace(); if (Part.Drops != 1) return 1; }
 return Part.Drops == 2 ? 42 : Part.Drops + 5; } }''', 'run:42'),
        'normal_factory_base_value_move_replace_twice': ('''struct Part { public static int Drops; public int Id;
 public Part(int id) { Id = id; } ~Part() { Drops = Drops + 1; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public static int AfterReplace; public Part field = new Part(1); public int Number;
 public Base() { Part first = move(field); field = new Part(2); Part second = move(field);
 field = new Part(3); AfterReplace = Part.Drops; Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } } class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Base.AfterReplace != 0 ? 4 : Part.Drops == 3 ? 42 : Part.Drops + 5; } }''', 'run:42'),
        'normal_factory_base_mixed_field_failure': ('''class Trace { public static int Value; }
 struct Part { public int Id; public Part(int id) { Id = id; } ~Part() { if (Id != 0) { Trace.Value = Trace.Value * 10 + Id; } } }
 class Item { public int Id; public Item(int id) { Id = id; } ~Item() { Trace.Value = Trace.Value * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public owned Item first = new Item(1); public Part second = new Part(2); public int Number;
 public Base() { Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Trace.Value == 21 ? 42 : Trace.Value; } }''', 'run:42'),
        'normal_factory_implicit_value_field_incomplete': ('''struct Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { if (Id != 0) { Trace = Trace * 10 + Id; } } }
 class Base { public Base() { } ~Base() { Part.Trace = Part.Trace * 10 + 1; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Middle : Base { public Part field = new Part(4); public int Number = Gate.Read();
 ~Middle() { Part.Trace = Part.Trace * 10 + 3; } }
 class Derived : Middle { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 41 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_implicit_value_field_completed': ('''struct Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { if (Id != 0) { Trace = Trace * 10 + Id; } } }
 class Base { public Base() { } ~Base() { Part.Trace = Part.Trace * 10 + 1; } }
 class Middle : Base { public Part field = new Part(4); ~Middle() { Part.Trace = Part.Trace * 10 + 3; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Derived : Middle { public int Number; public Derived() { Number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 341 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_base_argument_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public static int Started; public Base(owned Resource parameter, int value) { Started = 1; } }
 class Derived : Base { public Derived() : base(new Resource(), Gate.Read()) { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Base.Started != 0 ? 3 : Resource.Drops == 1 ? 42 : Resource.Drops == 0 ? 4 : 5; } }''', 'run:42'),
        'normal_factory_base_argument_failure_order': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public static int Started; public Base(owned Part first, owned Part second, int value) { Started = 1; } }
 class Derived : Base { public Derived() : base(new Part(1), new Part(2), Gate.Read()) { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Base.Started == 0 && Part.Trace == 21 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_base_argument_completed': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Base { public owned Part field; public Base(owned Part value) { field = move(value); }
 ~Base() { Part.Trace = Part.Trace * 10 + 2; } }
 class Derived : Base { public int Number; public Derived() : base(new Part(1)) { Number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 21 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_base_argument_success': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Base { public owned Part field; public Base(owned Part value) { field = move(value); }
 ~Base() { Part.Trace = Part.Trace * 10 + 2; } }
 class Derived : Base { public Derived() : base(new Part(1)) { } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (Part.Trace != 0) return 1; } return Part.Trace == 21 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_base_owned_destroy': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number;
 public Base() { resource = new Resource(); destroy resource; Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught != 1 ? 2 : Resource.Drops == 0 ? 3 : Resource.Drops == 1 ? 42 : 4; } }''',
                                               'explicit destroy of an owned value'),
        'normal_factory_base_owned_destroy_success': ('''class Base { public owned Resource resource;
 public Base() { resource = new Resource(); destroy resource; } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); }
 return Resource.Drops == 0 ? 3 : Resource.Drops == 1 ? 42 : 4; } }''',
                                                       'explicit destroy of an owned value'),
        'normal_factory_base_owned_branch_skipped': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number; public Base() {
 bool take = false; if (take) { resource = new Resource(); } Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 0 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_owned_before_store': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number; public Base() {
 Number = Gate.Read(); resource = new Resource(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 0 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_owned_loop': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Base { public owned Resource resource; public int Number; public Base() {
 int i = 0; while (i < 1) { resource = new Resource(); i = i + 1; } Number = Gate.Read(); } }
 class Derived : Base { public Derived() { } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Resource.Drops == 1 ? 42 : 1; } }''', 'run:42'),
        'normal_factory_base_caught': ('''class Base { public static int Drops;
 public Base() { } ~Base() { Drops = Drops + 1; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Derived : Base { public static int Caught; public Derived() {
 try { int value = Gate.Read(); } catch (Exception error) { Caught = 1; } } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (Derived.Caught != 1 || Base.Drops != 0) return 1; } return Base.Drops == 1 ? 42 : 2; } }''', 'run:42'),
        'normal_factory_implicit_base_throw': ('''class Base { public static int Trace;
 public Base() { } ~Base() { Trace = Trace * 10 + 1; } }
 class Middle : Base { ~Middle() { Trace = Trace * 10 + 2; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Derived : Middle { public int Number; public Derived() { Number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Base.Trace == 21 ? 42 : Base.Trace; } }''', 'run:42'),
        'normal_factory_implicit_base_incomplete': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Base { public owned Part baseField; public Base() { baseField = new Part(1); }
 ~Base() { Part.Trace = Part.Trace * 10 + 2; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Middle : Base { public owned Part middleField = new Part(4); public int marker = Gate.Read();
 ~Middle() { Part.Trace = Part.Trace * 10 + 3; } }
 class Derived : Middle { public Derived() { } ~Derived() { Part.Trace = Part.Trace * 10 + 6; } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 421 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_completed_base_helper': ('''class Part { public static int Trace;
 ~Part() { Trace = Trace * 10 + 1; } }
 class Base { public owned Part value; public void Initialize() { value = new Part(); }
 public Base() { } ~Base() { Part.Trace = Part.Trace * 10 + 2; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Derived : Base { public int Number; public Derived() { Initialize(); Number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 21 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_completed_base_helper_order': ('''class Part { public static int Trace; private int id;
 public Part(int value) { id = value; } ~Part() { Trace = Trace * 10 + id; } }
 class Base { public owned Part value; public void Initialize() { value = new Part(1); }
 public Base() { } ~Base() { Part.Trace = Part.Trace * 10 + 2; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Derived : Base { public owned Part other; public int Number;
 public Derived() { Initialize(); other = new Part(3); Number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 321 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_implicit_base_completed_owned': ('''class Part { public static int Trace; public int Id;
 public Part(int id) { Id = id; } ~Part() { Trace = Trace * 10 + Id; } }
 class Base { public owned Part baseField; public Base() { baseField = new Part(1); }
 ~Base() { Part.Trace = Part.Trace * 10 + 2; } }
 class Middle : Base { public owned Part middleField = new Part(4);
 ~Middle() { Part.Trace = Part.Trace * 10 + 3; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Derived : Middle { public int Number; public Derived() { Number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Part.Trace == 3421 ? 42 : Part.Trace; } }''', 'run:42'),
        'normal_factory_two_implicit_bases': ('''class Base { public static int Trace;
 public Base() { } ~Base() { Trace = Trace * 10 + 1; } }
 class Middle : Base { ~Middle() { Trace = Trace * 10 + 2; } }
 class Upper : Middle { ~Upper() { Trace = Trace * 10 + 3; } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Derived : Upper { public int Number; public Derived() { Number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { STRONG<Derived> owner = SharedPointers.MAKE<Derived>(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Base.Trace == 321 ? 42 : Base.Trace; } }''', 'run:42'),
        'normal_factory_implicit_base_success': ('''class Base { public static int Trace;
 public Base() { } ~Base() { Trace = Trace * 10 + 1; } }
 class Middle : Base { ~Middle() { Trace = Trace * 10 + 2; } }
 class Derived : Middle { public Derived() { } }
 class Program { public static int Main() { { STRONG<Derived> owner = SharedPointers.MAKE<Derived>();
 if (Base.Trace != 0) return 1; } return Base.Trace == 21 ? 42 : Base.Trace; } }''', 'run:42'),
        'struct_pattern_ref_escape': ('''class Program { public static long fallback;
 public static ref readonly long Leak(STRONG<Value> owner) {
 match (owner.Get()) { ref item => { return ref item.Data; } } return ref fallback;
 } public static int Main() { return 0; } }''', 'ref return cannot expose a by-value or readonly parameter'),
        'copy': ('''class Program { public static int Main() { STRONG<Payload> a = SharedPointers.MAKE<Payload>();
 STRONG<Payload> b = a; return 0; } }''', 'move'),
        'weak_copy': ('''class Program { public static int Main() { WEAK<Payload> a = default(WEAK<Payload>);
 WEAK<Payload> b = a; return 0; } }''', 'move'),
        'tuple_owner_copy': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 (STRONG<Payload>, int) pair = (owner, 42); return 0; } }''', 'move'),
        'tuple_owner_aggregate_copy': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 (STRONG<Payload>, int) pair = { owner, 42 }; return 0; } }''', 'move'),
        'tuple_owner_conditional_copy': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 (STRONG<Payload>, int) pair = (true ? owner : owner, 42); return 0; } }''', 'move'),
        'conditional_owner_copy': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 STRONG<Payload> copy = true ? owner : owner; return 0; } }''', 'move'),
        'conditional_owner_mixed': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 STRONG<Payload> copy = true ? owner.Clone() : owner; return 0; } }''', 'move'),
        'conditional_weak_copy': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); WEAK<Payload> weak = owner.Downgrade();
 WEAK<Payload> copy = true ? weak : weak; return 0; } }''', 'move'),
        'conditional_owner_clone': ('''class Program { public static int Main() {
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
   { STRONG<Payload> copy = true ? owner.Clone() : owner.Clone();
     if (owner.StrongCount() != 2l) return 1; }
   if (owner.StrongCount() != 1l) return 2;
 } return Payload.Drops == 1 ? 42 : 3; } }''', 'run:42'),
        'tuple_weak_copy': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); WEAK<Payload> weak = owner.Downgrade();
 (WEAK<Payload>, int) pair = (weak, 42); return 0; } }''', 'move'),
        'tuple_weak_aggregate_copy': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); WEAK<Payload> weak = owner.Downgrade();
 (WEAK<Payload>, int) pair = { weak, 42 }; return 0; } }''', 'move'),
        'tuple_owner_clone': ('''class Program { public static int Main() {
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
   { (STRONG<Payload>, int) pair = (owner.Clone(), 42); if (owner.StrongCount() != 2l) return 1; }
   if (owner.StrongCount() != 1l || Payload.Drops != 0) return 2;
 } return Payload.Drops == 1 ? 42 : 3; } }''', 'run:42'),
        'tuple_owner_aggregate_clone': ('''class Program { public static int Main() {
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
   { (STRONG<Payload>, int) pair = { owner.Clone(), 42 }; if (owner.StrongCount() != 2l) return 1; }
   if (owner.StrongCount() != 1l || Payload.Drops != 0) return 2;
 } return Payload.Drops == 1 ? 42 : 3; } }''', 'run:42'),
        'tuple_clone_exception_diagnostic': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 42; } }
class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); int caught = 0;
 try { (STRONG<Payload>, int) pair = (owner.Clone(), Gate.Read()); }
 catch (Exception error) { caught = 1; }
 if (caught != 1) return 3; return owner.StrongCount() == 1l ? 42 : 4; } }''', 'run:42'),
        'tuple_clone_try_success': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 try { (STRONG<Payload>, int) pair = (owner.Clone(), 42); }
 catch (Exception error) { return 3; }
 return owner.StrongCount() == 1l ? 42 : 4; } }''', 'run:42'),
        'tuple_aggregate_clone_try_success': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 try { (STRONG<Payload>, int) pair = { owner.Clone(), 42 }; }
 catch (Exception error) { return 3; }
 return owner.StrongCount() == 1l ? 42 : 4; } }''', 'run:42'),
        'tuple_aggregate_clone_exception_diagnostic': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 42; } }
class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); int caught = 0;
 try { (STRONG<Payload>, int) pair = { owner.Clone(), Gate.Read() }; }
 catch (Exception error) { caught = 1; }
 if (caught != 1) return 3; return owner.StrongCount() == 1l ? 42 : 4; } }''', 'run:42'),
        'tuple_weak_clone': ('''class Program { public static int Main() {
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); WEAK<Payload> weak = owner.Downgrade();
   { (WEAK<Payload>, int) pair = (weak.Clone(), 42); if (weak.WeakCount() != 2l) return 1; }
   if (weak.WeakCount() != 1l || Payload.Drops != 0) return 2;
 } return Payload.Drops == 1 ? 42 : 3; } }''', 'run:42'),
        'tuple_weak_aggregate_clone': ('''class Program { public static int Main() {
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); WEAK<Payload> weak = owner.Downgrade();
   { (WEAK<Payload>, int) pair = { weak.Clone(), 42 }; if (weak.WeakCount() != 2l) return 1; }
   if (weak.WeakCount() != 1l || Payload.Drops != 0) return 2;
 } return Payload.Drops == 1 ? 42 : 3; } }''', 'run:42'),
        'weak_empty': ('''class Program { public static int Main() { WEAK<Payload> weak = default(WEAK<Payload>);
 WEAK<Payload> clone = weak.Clone(); STRONG<Payload> owner = weak.Upgrade();
 return weak.Expired() && clone.Expired() && weak.WeakCount() == 0l && owner.IsEmpty() ? 42 : 1; } }''',
                       'run:42'),
        'strong_empty_lifecycle': ('''class Program { public static int Main() {
 STRONG<Payload> owner = default(STRONG<Payload>); STRONG<Payload> clone = owner.Clone();
 WEAK<Payload> weak = owner.Downgrade(); STRONG<Payload> upgraded = weak.Upgrade();
 return owner.IsEmpty() && clone.IsEmpty() && upgraded.IsEmpty() && weak.Expired() &&
 owner.StrongCount() == 0l && owner.WeakCount() == 0l && weak.WeakCount() == 0l && Payload.Drops == 0 ? 42 : 1;
} }''', 'run:42'),
        'move_assign_distinct_blocks': ('''class Program { public static int Main() {
 WEAK<Payload> firstWeak; WEAK<Payload> secondWeak;
 { STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
   firstWeak = first.Downgrade(); secondWeak = second.Downgrade(); second = move(first);
   if (Payload.Drops != 1 || !secondWeak.Expired() || firstWeak.Expired() || second.StrongCount() != 1l)
     return 1;
   STRONG<Payload> retained = firstWeak.Upgrade();
   if (retained.IsEmpty() || retained.StrongCount() != 2l || retained.Get().Data != 42l) return 2;
 }
 return Payload.Drops == 2 && firstWeak.Expired() && secondWeak.Expired() &&
        firstWeak.WeakCount() == 1l && secondWeak.WeakCount() == 1l ? 42 : 3;
} }''', 'run:42'),
        'static_strong': ('''class Holder { public static STRONG<Payload> value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_weak': ('''class Holder { public static WEAK<Payload> value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_nested_struct': ('''struct Inner { public WEAK<Payload> value; }
struct Outer { public Inner inner; }
class Holder { public static Outer value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_nested_generic': ('''struct Wrapper<T> { public T value; }
class Holder { public static Wrapper<WEAK<Payload>> value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_nested_tuple_weak': ('''class Holder { public static (int, WEAK<Payload>) value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_nested_tuple_strong': ('''struct Wrapper<T> { public T value; }
class Holder { public static (int, Wrapper<STRONG<Payload>>) value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_tuple_atomic_control': ('''class Holder { public static (int, WeakArc<Payload>) value; }
class Program { public static int Main() { return 42; } }''', 'run:42'),
        'static_tuple_atomic_replace': ('''class Holder { public static (int, WeakArc<Payload>) value; }
class Program { public static int Main() { Arc<Payload> owner = SharedPointers.MakeArc<Payload>();
 Holder.value = (0, owner.Downgrade()); if (owner.WeakCount() != 1l) return 1;
 Holder.value = (0, default(WeakArc<Payload>)); return owner.WeakCount() == 0l ? 42 : 2; } }''', 'run:42'),
        'static_tuple_atomic_replace_other': ('''class Holder { public static (int, WeakArc<Payload>) value; }
class Program { public static int Main() { Arc<Payload> first = SharedPointers.MakeArc<Payload>();
 Arc<Payload> second = SharedPointers.MakeArc<Payload>(); Holder.value = (0, first.Downgrade());
 if (first.WeakCount() != 1l || second.WeakCount() != 0l) return 1;
 Holder.value = (0, second.Downgrade()); if (first.WeakCount() != 0l || second.WeakCount() != 1l) return 2;
 Holder.value = (0, default(WeakArc<Payload>)); return second.WeakCount() == 0l ? 42 : 3; } }''', 'run:42'),
        'static_tuple_value_control': ('''struct ValueGuard { ~ValueGuard() { } }
class Holder { public static (int, ValueGuard) value; }
class Program { public static int Main() { return 42; } }''', 'run:42'),
        'static_tuple_scalar_control': ('''class Holder { public static (int, long) value; }
class Program { public static int Main() { return 42; } }''', 'run:42'),
        'static_tuple_scalar_with_local': ('''class Holder { public static (int, long) value; }
class Program { public static int Main() { (int, long) local = (1, 2l); return 42; } }''', 'run:42'),
        'static_tuple_scalar_read_write': ('''class Holder { public static (int, long) value; }
class Program { public static int Main() { Holder.value = (40, 2l);
 var (first, second) = Holder.value; return first + (int)second; } }''', 'run:42'),
        'static_nested_tuple_scalar': ('''class Holder { public static (int, (int, long)) value; }
class Program { public static int Main() { return 42; } }''', 'run:42'),
        'static_tuple_open_generic_control': ('''class Holder<T> { public static (int, T) value; }
class Program { public static int Main() { return 42; } }''', 'run:42'),
        'namespace_tuple_alias_control': ('''using Pair = (int, long);
namespace Stores { Pair value; } class Program { public static int Main() { return 42; } }''', 'run:42'),
        'namespace_tuple_alias_atomic': ('''using Pair = (int, WeakArc<Payload>);
namespace Stores { Pair value; } class Program { public static int Main() { return 42; } }''', 'run:42'),
        'namespace_tuple_alias_local': ('''using Pair = (int, WEAK<Payload>);
namespace Stores { Pair value; } class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'class_tuple_alias_control': ('''using Pair = (int, long);
class Holder { public static Pair value; } class Program { public static int Main() { return 42; } }''', 'run:42'),
        'local_tuple_alias_control': ('''using Pair = (int, long);
class Program { public static int Main() { Pair value = (40, 2l);
 var (first, second) = value; return first + (int)second; } }''', 'run:42'),
        'nested_tuple_alias_control': ('''using Pair = (int, (int, long)); using Other = Pair;
class Holder { public static Other value; } class Program { public static int Main() { return 42; } }''', 'run:42'),
        'direct_nested_tuple_alias_control': ('''using Pair = (int, (int, long));
class Holder { public static Pair value; } class Program { public static int Main() { return 42; } }''', 'run:42'),
        'named_tuple_alias_control': ('''using Pair = (int first, long second);
class Holder { public static Pair value; } class Program { public static int Main() { return 42; } }''', 'run:42'),
        'static_nested_arc': ('''class Holder { public static Arc<WEAK<Payload>> value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_nested_base': ('''class Base { public WEAK<Payload> value; }
class Derived : Base { } class Holder { public static Derived value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_polymorphic_container': ('''class Base { }
class Derived : Base { public WEAK<Payload> value; }
class Holder { public static Base value; }
class Program { public static int Main() { Holder.value = new Derived(); return 0; } }''', 'thread-local shared'),
        'static_interface_container': ('''interface IValue { }
class Derived : IValue { public WEAK<Payload> value; }
class Holder { public static IValue value; }
class Program { public static int Main() { Holder.value = new Derived(); return 0; } }''', 'thread-local shared'),
        'static_array': ('''class Holder { public static WEAK<Payload>[] value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_local': ('''class Program { public static int Main() {
 static WEAK<Payload> value; return 0; } }''', 'thread-local shared'),
        'namespace_state': ('''namespace Stores { WEAK<Payload> value; }
class Program { public static int Main() { return 0; } }''', 'thread-local shared'),
        'static_atomic_allowed': ('''class Holder { public static WeakArc<Payload> value; }
class Program { public static int Main() { return Holder.value.Expired() ? 42 : 1; } }''', 'run:42'),
        'static_other_rc_allowed': ('''namespace Other { public struct Rc<T> { public int value; } }
class Holder { public static Other.Rc<Payload> value; }
class Program { public static int Main() { return 42; } }''', 'run:42'),
        'static_cycle_allowed': ('''class Node { public Node next; }
class Holder { public static Node value; }
class Program { public static int Main() { return 42; } }''', 'run:42'),
        'static_sibling_allowed': ('''class Base { } class Dangerous : Base { public WEAK<Payload> value; }
class Safe : Base { } class Holder { public static Safe value; }
class Program { public static int Main() { return 42; } }''', 'run:42'),
        'local_reentrant_weak_count': ('''class Trace { public static nint WeakAddress; public static long Count; }
class Reentrant { ~Reentrant() { unsafe("single-threaded test observer") {
 WEAK<Reentrant>* weak = (WEAK<Reentrant>*)Trace.WeakAddress; Trace.Count = (*weak).WeakCount(); } } }
class Program { public static int Main() {
 WEAK<Reentrant> weak; unsafe("single-threaded test observer") { Trace.WeakAddress = (nint)&weak; }
 { STRONG<Reentrant> owner = SharedPointers.MAKE<Reentrant>(); weak = owner.Downgrade(); }
 return Trace.Count == 1l && weak.Expired() && weak.WeakCount() == 1l ? 42 : 1;
} }''', 'run:42'),
        'atomic_reentrant_weak_count': ('''class Trace { public static WEAK<Reentrant> Weak; public static long Count; }
class Reentrant { ~Reentrant() { Trace.Count = Trace.Weak.WeakCount(); } }
class Program { public static int Main() {
 { STRONG<Reentrant> owner = SharedPointers.MAKE<Reentrant>(); Trace.Weak = owner.Downgrade(); }
 return Trace.Count == 1l && Trace.Weak.Expired() && Trace.Weak.WeakCount() == 1l ? 42 : 1;
} }''', 'run:42'),
        'expired_weak_clone_move': ('''class Program { public static int Main() {
 WEAK<Payload> survivor;
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
   WEAK<Payload> original = owner.Downgrade(); survivor = move(original); }
 if (!survivor.Expired() || survivor.WeakCount() != 1l || Payload.Drops != 1) return 1;
 { WEAK<Payload> clone = survivor.Clone(); WEAK<Payload> moved = move(clone);
   STRONG<Payload> dead = moved.Upgrade();
   if (!moved.Expired() || !dead.IsEmpty() || survivor.WeakCount() != 2l || Payload.Drops != 1) return 2;
 }
 return survivor.WeakCount() == 1l && Payload.Drops == 1 ? 42 : 3;
} }''', 'run:42'),
        'weak_field_helper_constructor_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public WEAK<Payload> value; public int number;
 public void Initialize(WEAK<Payload> weak) { value = move(weak); }
 public Holder(WEAK<Payload> weak) { Initialize(move(weak)); number = Gate.Read(); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Downgrade()); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.WeakCount() == 0l ? 42 : (int)owner.WeakCount() + 10 * caught; } }''', 'run:42'),
        'weak_field_helper_itself_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public WEAK<Payload> value; public int number;
 public void Initialize(WEAK<Payload> weak) { value = move(weak); number = Gate.Read(); }
 public Holder(WEAK<Payload> weak) { Initialize(move(weak)); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Downgrade()); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.WeakCount() == 0l ? 42 : (int)owner.WeakCount() + 10 * caught; } }''', 'run:42'),
        'weak_field_helper_move_out_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public WEAK<Payload> value; public int number;
 public void Initialize(WEAK<Payload> weak) { value = move(weak); }
 public void Release() { WEAK<Payload> taken = move(value); }
 public Holder(WEAK<Payload> weak) { Initialize(move(weak)); Release(); number = Gate.Read(); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Downgrade()); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.WeakCount() == 0l ? 42 : (int)owner.WeakCount() + 10 * caught; } }''', 'run:42'),
        'weak_field_empty_constructor_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public WEAK<Payload> value; public int number; public Holder() { number = Gate.Read(); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.WeakCount() == 0l ? 42 : (int)owner.WeakCount() + 10 * caught; } }''', 'run:42'),
        'strong_field_helper_constructor_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public STRONG<Payload> value; public int number;
 public void Initialize(STRONG<Payload> strong) { value = move(strong); }
 public Holder(STRONG<Payload> strong) { Initialize(move(strong)); number = Gate.Read(); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Clone()); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.StrongCount() == 1l ? 42 : (int)owner.StrongCount() + 10 * caught; } }''', 'run:42'),
        'strong_field_helper_itself_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public STRONG<Payload> value; public int number;
 public void Initialize(STRONG<Payload> strong) { value = move(strong); number = Gate.Read(); }
 public Holder(STRONG<Payload> strong) { Initialize(move(strong)); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Clone()); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.StrongCount() == 1l ? 42 : (int)owner.StrongCount() + 10 * caught; } }''', 'run:42'),
        'unrelated_weak_default_preserved': ('''class Tracker { public static int Drops; }
 namespace Other { public struct WeakRc<T> { public void Touch() { } ~WeakRc() { Tracker.Drops = Tracker.Drops + 1; } } }
 class Gate { static Gate() throws Exception { throw new Exception(); } public static int Read() { return 7; } }
 class Holder { public Other.WeakRc<Payload> value; public int number;
 public Holder() { value.Touch(); number = Gate.Read(); } }
 class Program { public static int Main() { int caught = 0;
 try { Holder holder = new Holder(); } catch (Exception error) { caught = 1; }
 return caught == 1 && Tracker.Drops == 0 ? 42 : Tracker.Drops + 10 * caught; } }''', 'run:42'),
        'weak_field_move_reinitialize_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public WEAK<Payload> value; public int number;
 public void Initialize(WEAK<Payload> weak) { value = move(weak); }
 public Holder(WEAK<Payload> first, WEAK<Payload> second) {
 value = move(first); { WEAK<Payload> taken = move(value); }
 Initialize(move(second)); number = Gate.Read(); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Downgrade(), owner.Downgrade()); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && owner.WeakCount() == 0l ? 42 : (int)owner.WeakCount() + 10 * caught; } }''', 'run:42'),
        'strong_field_move_reinitialize_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public STRONG<Payload> value; public int number;
 public void Initialize(STRONG<Payload> strong) { value = move(strong); }
 public Holder(STRONG<Payload> first, STRONG<Payload> second) {
 value = move(first); { STRONG<Payload> taken = move(value); }
 Initialize(move(second)); number = Gate.Read(); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Clone(), owner.Clone()); }
 catch (Exception error) { caught = 1; }
 return caught == 1 && owner.StrongCount() == 1l ? 42 : (int)owner.StrongCount() + 10 * caught; } }''', 'run:42'),
        'weak_field_direct_constructor_failure': ('''class Gate { static Gate() throws Exception { throw new Exception(); }
 public static int Read() { return 7; } }
 class Holder { public WEAK<Payload> value; public int number;
 public Holder(WEAK<Payload> weak) { value = move(weak); number = Gate.Read(); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 int caught = 0; try { Holder holder = new Holder(owner.Downgrade()); } catch (Exception error) { caught = 1; }
 return caught == 1 && owner.WeakCount() == 0l ? 42 : (int)owner.WeakCount() + 10 * caught; } }''', 'run:42'),
        'weak_field_helper_constructor_success': ('''class Holder { public WEAK<Payload> value;
 public void Initialize(WEAK<Payload> weak) { value = move(weak); }
 public Holder(WEAK<Payload> weak) { Initialize(move(weak)); } }
 class Program { public static int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 { owned Holder holder = new Holder(owner.Downgrade()); if (owner.WeakCount() != 1l) return 1; }
 return owner.WeakCount() == 0l ? 42 : 2; } }''', 'run:42'),
        'escape': ('''class Program { public static int Main() { SharedView<Payload> view;
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); view = owner.Borrow(); }
 return (int)view.Get().Data; } }''', 'longer-lived'),
        'view_struct_field_escape': ('''struct Holder { public SharedView<Payload> view; }
class Program { public static int Main() { Holder holder = default(Holder);
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); holder.view = owner.Borrow(); }
 return (int)holder.view.Get().Data; } }''', 'borrow'),
        'view_tuple_escape': ('''class Program { public static int Main() {
 (SharedView<Payload>, int) saved; { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 saved = (owner.Borrow(), 42); } return 0; } }''', 'borrow'),
        'view_tuple_deconstruct_after_owner': ('''class Program { public static int Main() {
 (SharedView<Payload>, int) saved; { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 saved = (owner.Borrow(), 42); } var (view, number) = saved; return (int)view.Get().Data + number; } }''',
                                               'borrow'),
        'view_tuple_deconstruct_live': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); (SharedView<Payload>, int) saved = (owner.Borrow(), 0);
 var (view, number) = saved; return (int)view.Get().Data + number; } }''', 'run:42'),
        'view_array_escape': ('''class Program { public static int Main() {
 SharedView<Payload>[] saved = new SharedView<Payload>[1];
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); saved[0] = owner.Borrow(); }
 return 0; } }''', 'borrow'),
        'mutate': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 alias.Data = 10l; return 0; } }''', 'not assignable'),
        'cast_readonly_get': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 borrowed Payload converted = (Payload)alias; converted.Data = 10l; return 0; } }''', 'borrowed local requires'),
        'cast_readonly_direct': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 ((Payload)owner.Get()).Data = 10l; return 0; } }''', 'not assignable'),
        'cast_readonly_user_operator': ('''class CastPayload { public int Data;
 public static explicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Main() {
 STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 return (int)owner.Get(); } }''', 'readonly borrowed'),
        'implicit_readonly_user_operator': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Main() {
 STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 int result = owner.Get(); return owner.Get().Data == 9 ? 42 : 1; } }''', 'readonly borrowed'),
        'implicit_readonly_assignment': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Main() {
 STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>(); int result = 0;
 result = owner.Get(); return result; } }''', 'readonly borrowed'),
        'implicit_readonly_return': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Work(STRONG<CastPayload> owner) { return owner.Get(); }
 public static int Main() { return Work(SharedPointers.MAKE<CastPayload>()); } }''', 'borrowed value cannot escape'),
        'implicit_readonly_argument': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Use(int value) { return value; } public static int Main() {
 STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>(); return Use(owner.Get()); } }''',
                                       'readonly borrowed'),
        'implicit_readonly_property': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Sink { public int Value { get; set; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 Sink sink = new Sink(); sink.Value = owner.Get(); return sink.Value; } }''', 'borrowed value cannot be stored'),
        'implicit_readonly_array': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 int[] values = new int[1]; values[0] = owner.Get(); return values[0]; } }''', 'readonly borrowed'),
        'implicit_readonly_condition': ('''class CastPayload { public int Data;
 public static implicit operator bool(CastPayload value) { value.Data = 9; return true; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 if (owner.Get()) return 1; return 0; } }''', 'condition'),
        'implicit_readonly_null_coalesce_assign': ('''class CastPayload { public int Data;
 public static implicit operator string(CastPayload value) { value.Data = 9; return "ok"; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 string result = null; result ??= owner.Get(); return owner.Get().Data == 9 ? 42 : 1; } }''', 'readonly borrowed'),
        'implicit_null_coalesce_mutable_control': ('''class CastPayload { public static int Calls;
 public static implicit operator string(CastPayload value) { Calls = Calls + 1; return "ok"; } }
 class Program { public static int Main() { CastPayload item = new CastPayload(); borrowed CastPayload alias = item;
 string first = null; first ??= alias; string second = "set"; second ??= alias;
 return CastPayload.Calls == 1 ? 42 : 1; } }''', 'run:42'),
        'implicit_readonly_null_coalesce': ('''class CastPayload { public int Data;
 public static implicit operator string(CastPayload value) { value.Data = 9; return "ok"; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 string value = null; value ?? owner.Get(); return owner.Get().Data == 9 ? 42 : 1; } }''', 'readonly borrowed'),
        'implicit_null_coalesce_expression_mutable_control': ('''class CastPayload { public static int Calls;
 public static implicit operator string(CastPayload value) { Calls = Calls + 1; return "ok"; } }
 class Program { public static int Main() { CastPayload item = new CastPayload(); borrowed CastPayload alias = item;
 string first = null; first ?? alias; string second = "set"; second ?? alias;
 return CastPayload.Calls == 1 ? 42 : 1; } }''', 'run:42'),
        'implicit_readonly_collection_probe': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 int[] values = [owner.Get()]; return owner.Get().Data == 9 ? 42 : 1; } }''', 'readonly borrowed'),
        'readonly_collection_alias_probe': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); Payload[] values = [owner.Get()];
 values[0].Data = 9l; return owner.Get().Data == 9l ? 42 : 1; } }''', 'readonly borrowed'),
        'readonly_collection_spread_alias_probe': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); Payload[] existing = new Payload[0];
 Payload[] values = [..existing, owner.Get()]; values[0].Data = 9l;
 return owner.Get().Data == 9l ? 42 : 1; } }''', 'readonly borrowed'),
        'readonly_collection_spread_source_probe': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>();
 Child[] values = [..owner.Get().Data]; values[0].Value = 9;
 return owner.Get().Data[0].Value == 9 ? 42 : 1; } }''', 'readonly borrowed'),
        'implicit_readonly_collection_spread_probe': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 int[] existing = [1]; int[] values = [..existing, owner.Get()];
 return owner.Get().Data == 9 ? 42 : 1; } }''', 'readonly borrowed'),
        'implicit_readonly_collection_builder_probe': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Bucket { public static owned Bucket CreateBuilder() { return new Bucket(); }
 public void Add(int value) { } public Bucket ToCollection() { return this; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 Bucket result = [owner.Get()]; return owner.Get().Data == 9 ? 42 : 1; } }''', 'readonly borrowed'),
        'readonly_collection_numeric_spread_control': ('''class Program { public static int Main() {
 STRONG<ArrayPayload> owner = SharedPointers.MAKE<ArrayPayload>(); int[] values = [..owner.Get().Data];
 return values.Length == 1 && values[0] == 0 ? 42 : 1; } }''', 'run:42'),
        'collection_mutable_conversion_control': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 42; return value.Data; } }
 class Program { public static int Main() { CastPayload item = new CastPayload(); borrowed CastPayload alias = item;
 int[] values = [alias]; return values[0] == 42 && item.Data == 42 ? 42 : 1; } }''', 'run:42'),
        'collection_builder_plain_probe': ('''class Bucket { public static int Sum;
 public static owned Bucket CreateBuilder() { return new Bucket(); }
 public void Add(int value) { Sum = Sum + value; } public Bucket ToCollection() { return this; } }
 class Program { public static int Main() { Bucket result = [42]; return Bucket.Sum; } }''', 'run:42'),
        'collection_builder_numeric_probe': ('''class Bucket { public static int Sum;
 public static owned Bucket CreateBuilder() { return new Bucket(); }
 public void Add(int value) { Sum = Sum + value; } public Bucket ToCollection() { return this; } }
 class Program { public static int Main() { short item = 21; Bucket result = [item, 21]; return Bucket.Sum; } }''',
                                            'run:42'),
        'collection_builder_conversion_probe': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 42; return value.Data; } }
 class Bucket { public static int Sum; public static owned Bucket CreateBuilder() { return new Bucket(); }
 public void Add(int value) { Sum = Sum + value; } public Bucket ToCollection() { return this; } }
 class Program { public static int Main() { CastPayload item = new CastPayload(); borrowed CastPayload alias = item;
 Bucket result = [alias]; return item.Data == 42 ? Bucket.Sum : 1; } }''', 'run:42'),
        'collection_builder_conversion_order': ('''class Trace { public static int Value; }
 class Input { public int Id; public Input(int id) { Id = id; }
 public static implicit operator int(Input value) { Trace.Value = Trace.Value * 10 + value.Id; return value.Id; } }
 class Bucket { public static owned Bucket CreateBuilder() { return new Bucket(); }
 public void Add(int value) { Trace.Value = Trace.Value * 10 + value + 2; }
 public Bucket ToCollection() { return this; } }
 class Program { public static int Main() { Input first = new Input(1); Input second = new Input(2);
 borrowed Input a = first; borrowed Input b = second; Bucket result = [a, b];
 return Trace.Value == 1324 ? 42 : 1; } }''', 'run:42'),
        'implicit_readonly_range_probe': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 9; return 0; } }
 class Program { public static int Main() { STRONG<CastPayload> owner = SharedPointers.MAKE<CastPayload>();
 int[] values = new int[2]; values[owner.Get()..1]; return owner.Get().Data == 9 ? 42 : 1; } }''',
                                          'range endpoint requires an integral index'),
        'implicit_mutable_control': ('''class CastPayload { public int Data;
 public static implicit operator int(CastPayload value) { value.Data = 42; return value.Data; } }
 class Program { public static int Main() { CastPayload item = new CastPayload();
 borrowed CastPayload alias = item; int first = alias; int second = 0; second = alias;
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); int data = (int)owner.Get().Data;
 return first == 42 && second == 42 && data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_constructor_publishes_this': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Alias = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_destructor_publishes_this': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { } ~EscapePayload() { Alias = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_throwing_constructor_publishes_this': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() throws Exception { Alias = this; } }
 class Program { public static int Main() { try {
 STRONG<EscapePayload> owner = SharedPointers.MAKE_THROWING<EscapePayload>(); return 42;
 } catch (Exception error) { return 1; } } }''', 'emit:shared payload publishes its receiver'),
        'shared_base_constructor_publishes_this': ('''class BasePayload { public static BasePayload Alias;
 public BasePayload() { Alias = this; } }
 class EscapePayload : BasePayload { public EscapePayload() { } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_constructor_local_alias_probe': ('''class EscapePayload { public static EscapePayload Alias;
 public int Data; public EscapePayload() { EscapePayload local = this; Alias = local; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 EscapePayload.Alias.Data = 9; return owner.Get().Data == 9 ? 42 : 1; } }''',
                                                'emit:shared payload publishes its receiver'),
        'shared_destructor_local_alias': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { } ~EscapePayload() { EscapePayload local = this; Alias = local; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_constructor_alias_chain': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { EscapePayload first = null; first = this; EscapePayload second = first; Alias = second; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_constructor_receiver_helper': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Publish(); } public void Publish() { Alias = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_destructor_receiver_helper': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { } ~EscapePayload() { Publish(); } public void Publish() { Alias = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_constructor_virtual_helper': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Publish(); } public virtual void Publish() { Alias = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_destructor_virtual_helper': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { } ~EscapePayload() { Publish(); } public virtual void Publish() { Alias = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_base_virtual_override': ('''class BasePayload { public BasePayload() { Publish(); }
 public virtual void Publish() { } }
 class EscapePayload : BasePayload { public static BasePayload Alias; public EscapePayload() { }
 public override void Publish() { Alias = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_virtual_helper_control': ('''class SafePayload { public int Data;
 public SafePayload() { Inspect(); } public virtual void Inspect() { Data = 42; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_constructor_static_argument': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Publish(this); } public static void Publish(EscapePayload value) { Alias = value; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_global_field_constructor': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public EscapePayload() { Box.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_global_field_destructor': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public EscapePayload() { } ~EscapePayload() { Box.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_global_field_control': ('''class Holder { public int Value; }
 class SafePayload { public static Holder Box; public int Data; public SafePayload() { Box.Value = 1; Data = 42; } }
 class Program { public static int Main() { SafePayload.Box = new Holder();
 STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>(); return owner.Get().Data; } }''', 'run:42'),
        'shared_global_field_local_constructor': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public EscapePayload() { Holder alias = Box; alias.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_global_field_local_destructor': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public EscapePayload() { }
 ~EscapePayload() { Holder first = Box; Holder second = first; second.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_global_field_local_control': ('''class Holder { public SafePayload Value; }
 class SafePayload { public int Data; public SafePayload() { Holder local = new Holder(); local.Value = this; Data = 42; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data; } }''', 'run:42'),
        'shared_global_nested_constructor': ('''class Inner { public EscapePayload Value; }
 class Holder { public Inner Child; } class EscapePayload { public static Holder Box;
 public EscapePayload() { Box.Child.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_global_nested_destructor': ('''class Inner { public EscapePayload Value; }
 class Holder { public Inner Child; } class EscapePayload { public static Holder Box; public EscapePayload() { }
 ~EscapePayload() { Holder local = Box; Inner child = local.Child; child.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_local_nested_control': ('''class Inner { public SafePayload Value; }
 class Holder { public Inner Child; } class SafePayload { public int Data; public SafePayload() {
 Holder local = new Holder(); local.Child = new Inner(); local.Child.Value = this; Data = 42; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data; } }''', 'run:42'),
        'shared_global_array_constructor': ('''class EscapePayload { public static EscapePayload[] Box;
 public EscapePayload() { Box[0] = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_global_array_local_destructor': ('''class EscapePayload { public static EscapePayload[] Box;
 public EscapePayload() { } ~EscapePayload() { EscapePayload[] local = Box; local[0] = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_local_array_control': ('''class SafePayload { public int Data; public SafePayload() {
 SafePayload[] local = new SafePayload[1]; local[0] = this; Data = 42; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data; } }''', 'run:42'),
        'shared_called_holder_constructor': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public static Holder GetBox() { return Box; }
 public EscapePayload() { GetBox().Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_called_holder_local_destructor': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public static Holder GetBox() { return Box; }
 public EscapePayload() { } ~EscapePayload() { Holder local = GetBox(); local.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_called_holder_fresh_limit': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Identity(Holder input) { return input; }
 public EscapePayload() { Holder fresh = new Holder(); Holder local = Identity(fresh); local.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'run:42'),
        'shared_called_holder_identity_global': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public static Holder Identity(Holder input) { return input; }
 public EscapePayload() { Holder local = Identity(Box); local.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_called_holder_identity_published': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public static Holder Identity(Holder input) { return input; }
 public EscapePayload() { Holder fresh = new Holder(); Holder alias = Identity(fresh); Box = Identity(alias);
 Holder local = Identity(fresh); local.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_called_holder_identity_impure': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public static Holder Publish(Holder input) { Box = input; return input; }
 public EscapePayload() { Holder fresh = new Holder(); Holder local = Publish(fresh); local.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_called_holder_identity_prior_call': ('''class Holder { public EscapePayload Value; }
 class EscapePayload { public static Holder Box; public static Holder Identity(Holder input) { return input; }
 public static void Publish(Holder input) { Box = input; }
 public EscapePayload() { Holder fresh = new Holder(); Publish(fresh); Holder local = Identity(fresh); local.Value = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_static_property_publication': ('''class EscapePayload { public static EscapePayload Alias;
 public static EscapePayload Published { get { return Alias; } set { Alias = value; } }
 public EscapePayload() { Published = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_destructor_static_chain': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { } ~EscapePayload() { Forward(1, this); }
 public static void Forward(int ignored, EscapePayload value) { EscapePayload local = value; Publish(local); }
 public static void Publish(EscapePayload value) { Alias = value; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_static_argument_control': ('''class SafePayload { public int Data;
 public SafePayload() { Inspect(this); } public static void Inspect(SafePayload value) { value.Data = 42; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_named_static_argument': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Publish(value: this, ignored: 1); }
 public static void Publish(int ignored, EscapePayload value) { Alias = value; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_named_static_control': ('''class SafePayload { public int Data;
 public SafePayload() { Inspect(value: this, ignored: 1); }
 public static void Inspect(int ignored, SafePayload value) { value.Data = 42; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_constructor_instance_argument': ('''class Helper { public void Publish(EscapePayload value) { EscapePayload.Alias = value; } }
 class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Helper helper = new Helper(); helper.Publish(this); } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_named_instance_argument': ('''class Helper { public void Publish(int ignored, EscapePayload value) { EscapePayload.Alias = value; } }
 class EscapePayload { public static EscapePayload Alias; public EscapePayload() { Helper helper = new Helper();
 helper.Publish(value: this, ignored: 1); } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_destructor_instance_argument': ('''class Helper { public void Publish(int ignored, EscapePayload value) { EscapePayload.Alias = value; } }
 class EscapePayload { public static EscapePayload Alias; public Helper helper;
 public EscapePayload() { helper = new Helper(); } ~EscapePayload() { helper.Publish(1, this); } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_instance_argument_control': ('''class Helper { public void Inspect(SafePayload value) { value.Data = 42; } }
 class SafePayload { public int Data; public SafePayload() { Helper helper = new Helper(); helper.Inspect(this); } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_virtual_argument': ('''class Helper { public virtual void Publish(EscapePayload value) { EscapePayload.Alias = value; } }
 class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Helper helper = new Helper(); helper.Publish(this); } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_virtual_argument_override': ('''class Helper { public virtual void Publish(EscapePayload value) { } }
 class Derived : Helper { public override void Publish(EscapePayload value) { EscapePayload.Alias = value; } }
 class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Helper helper = new Derived(); helper.Publish(this); } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_virtual_argument_control': ('''class Helper { public virtual void Inspect(SafePayload value) { value.Data = 42; } }
 class Unrelated { public static SafePayload Alias; public virtual void Inspect(SafePayload value) { Alias = value; } }
 class SafePayload { public int Data; public SafePayload() { Helper helper = new Helper(); helper.Inspect(this); } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_interface_argument': ('''interface IPublisher { void Publish(EscapePayload value); }
 class Helper : IPublisher { public void Publish(EscapePayload value) { EscapePayload.Alias = value; } }
 class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Helper helper = new Helper(); borrowed IPublisher view = helper; view.Publish(this); } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_interface_argument_control': ('''interface IInspector { void Inspect(SafePayload value); }
 interface IUnrelated { void Inspect(SafePayload value); }
 class Helper : IInspector { public void Inspect(SafePayload value) { value.Data = 42; } }
 class Other : IUnrelated { public static SafePayload Alias; public void Inspect(SafePayload value) { Alias = value; } }
 class SafePayload { public int Data; public SafePayload() { Helper helper = new Helper();
 borrowed IInspector view = helper; view.Inspect(this); } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_interface_receiver': ('''interface IPublisher { void Publish(); }
 class EscapePayload : IPublisher { public static EscapePayload Alias;
 public EscapePayload() { Forward(this); } public static void Forward(IPublisher view) { view.Publish(); }
 public void Publish() { Alias = this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_interface_receiver_control': ('''interface IInspector { void Inspect(); }
 class SafePayload : IInspector { public int Data; public SafePayload() { Forward(this); }
 public static void Forward(IInspector view) { view.Inspect(); } public void Inspect() { Data = 42; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_receiver_return': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Alias = Self(); } public EscapePayload Self() { return this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_property_receiver_return': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Alias = Self; } public EscapePayload Self { get { return this; } } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_receiver_return_chain': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { EscapePayload local = First(); Alias = local; }
 public EscapePayload First() { return Second(); } public EscapePayload Second() { return this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_static_return': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Alias = Identity(this); }
 public static EscapePayload Identity(EscapePayload value) { return value; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_static_return_named_chain': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { EscapePayload local = First(value: this, ignored: null); Alias = local; }
 public static EscapePayload First(EscapePayload ignored, EscapePayload value) { return Second(value); }
 public static EscapePayload Second(EscapePayload value) { return value; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_static_return_multiple': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Alias = Pick(null, this); }
 public static EscapePayload Pick(EscapePayload first, EscapePayload second) { if (first == null) return second; return first; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_static_return_control': ('''class SafePayload { public static SafePayload Other; public int Data;
 public SafePayload() { Other = Ignore(this); Data = 42; }
 public static SafePayload Ignore(SafePayload value) { return null; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_virtual_receiver_return': ('''class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Alias = Self(); } public virtual EscapePayload Self() { return this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_virtual_override_return': ('''class BasePayload { public static BasePayload Alias;
 public BasePayload() { Alias = Self(); } public virtual BasePayload Self() { return null; } }
 class EscapePayload : BasePayload { public EscapePayload() { } public override BasePayload Self() { return this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_virtual_parameter_return': ('''class Helper { public virtual EscapePayload Identity(EscapePayload value) { return value; } }
 class EscapePayload { public static EscapePayload Alias;
 public EscapePayload() { Helper helper = new Helper(); Alias = helper.Identity(this); } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_virtual_return_control': ('''class Unrelated { public virtual SafePayload Find() { return null; } }
 class SafePayload { public static SafePayload Other; public int Data;
 public SafePayload() { Other = Find(); Data = 42; } public virtual SafePayload Find() { return null; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_virtual_recursive_return_control': ('''class SafePayload { public static SafePayload Other; public int Data;
 public SafePayload() { Other = Find(); Data = 42; }
 public virtual SafePayload Find() { if (Data < 0) return Find(); return null; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_interface_receiver_return': ('''interface IReturn { EscapePayload Self(); }
 class EscapePayload : IReturn { public static EscapePayload Alias;
 public EscapePayload() { Alias = Forward(this); } public static EscapePayload Forward(IReturn value) { return value.Self(); }
 public EscapePayload Self() { return this; } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_interface_parameter_return': ('''interface IReturn { EscapePayload Identity(EscapePayload value); }
 class Helper : IReturn { public EscapePayload Identity(EscapePayload value) { return value; } }
 class EscapePayload { public static EscapePayload Alias; public EscapePayload() { Helper helper = new Helper();
 borrowed IReturn view = helper; Alias = view.Identity(this); } }
 class Program { public static int Main() { STRONG<EscapePayload> owner = SharedPointers.MAKE<EscapePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'emit:shared payload publishes its receiver'),
        'shared_interface_return_control': ('''interface IReturn { SafePayload Self(); }
 interface IOther { SafePayload Self(); } class Other : IOther { public SafePayload Self() { return null; } }
 class SafePayload : IReturn { public static SafePayload Other; public int Data;
 public SafePayload() { Other = Forward(this); Data = 42; }
 public static SafePayload Forward(IReturn value) { return value.Self(); } public SafePayload Self() { return null; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_other_return_control': ('''class SafePayload { public static SafePayload Other; public int Data;
 public SafePayload() { Other = CreateOther(); Data = 42; } public SafePayload CreateOther() { return null; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_recursive_return_control': ('''class SafePayload { public static SafePayload Other; public int Data;
 public SafePayload() { Other = Find(); Data = 42; }
 public SafePayload Find() { if (Data < 0) return Find(); return null; } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.Get().Data == 42 ? 42 : 1; } }''', 'run:42'),
        'shared_receiver_recursive_helper_control': ('''class SafePayload { public int Data;
 public SafePayload() { Inspect(); } public void Inspect() { if (Data < 0) Inspect(); } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'run:42'),
        'shared_other_receiver_control': ('''class Helper { public static Helper Alias;
 public void Publish() { Alias = this; } }
 class SafePayload { public SafePayload() { Helper other = new Helper(); other.Publish(); } }
 class Program { public static int Main() { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 return owner.IsEmpty() ? 1 : 42; } }''', 'run:42'),
        'shared_receiver_field_control': ('''class SafePayload { public static int Drops; public int Data;
 public SafePayload() { this.Data = 42; } ~SafePayload() { Drops = Drops + 1; } }
 class Program { public static int Main() { { STRONG<SafePayload> owner = SharedPointers.MAKE<SafePayload>();
 if (owner.Get().Data != 42) return 1; } return SafePayload.Drops == 1 ? 42 : 2; } }''', 'run:42'),
        'ordinary_receiver_publication_control': ('''class PayloadControl { public static PayloadControl Alias;
 public int Data; public PayloadControl() { Alias = this; Data = 42; } }
 class Program { public static int Main() { PayloadControl value = new PayloadControl();
 return PayloadControl.Alias.Data == 42 ? 42 : 1; } }''', 'run:42'),
        'async_mutate_after_await': ('''class Program { public static async int Slow() { yield; return 7; }
 public static async int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 borrowed Payload alias = owner.Get(); int value = await Slow(); alias.Data = 10l; return value; } }''',
                                     'not assignable'),
        'async_move_after_await': ('''class Program { public static async int Slow() { yield; return 7; }
 public static async int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = owner.Borrow(); int value = await Slow();
 STRONG<Payload> moved = move(owner); return (int)view.Get().Data + value; } }''',
                                   'cannot move owner'),
        'async_read_after_await': ('''class Program { public static async int Slow() { yield; return 7; }
 public static async int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = owner.Borrow(); int value = await Slow();
 return view.Get().Data == 42l && value == 7 ? 42 : 1; } }''', 'run:42'),
        'tuple_alias_escape': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 (Payload, int) saved = (alias, 42); return 0; } }''', 'borrow'),
        'tuple_direct_escape': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 (Payload, int) saved = (owner.Get(), 42); return 0; } }''', 'borrow'),
        'tuple_aggregate_escape': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 (Payload, int) saved = { alias, 42 }; return 0; } }''', 'borrow'),
        'struct_aggregate_escape': ('''struct Carrier { public Payload Item; }
class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 Carrier saved = { alias }; return 0; } }''', 'borrow'),
        'array_element_initializer_escape': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 Payload[] saved = new Payload[] { alias }; return 0; } }''', 'borrow'),
        'array_element_assignment_escape': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 Payload[] saved = new Payload[1]; saved[0] = alias; return 0; } }''', 'borrow'),
        'property_assignment_escape': ('''class Holder { public Payload Value { get; set; } }
class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 Holder saved = new Holder(); saved.Value = alias; return 0; } }''',
                                       'borrowed value cannot be stored through a property/indexer setter'),
        'indexer_assignment_escape': ('''class Holder { private Payload item;
 public Payload this[int index] { get { return item; } set { item = value; } } }
class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 Holder saved = new Holder(); saved[0] = alias; return 0; } }''',
                                      'borrowed value cannot be stored through a property/indexer setter'),
        'mutate_array_field': ('''class Program { public static int Main() {
 STRONG<ArrayPayload> owner = SharedPointers.MAKE<ArrayPayload>();
 borrowed ArrayPayload alias = owner.Get(); alias.Data[0] = 10; return 0; } }''', 'not assignable'),
        'escape_array_field': ('''class Program { public static int Main() {
 int[] escaped; { STRONG<ArrayPayload> owner = SharedPointers.MAKE<ArrayPayload>();
 borrowed ArrayPayload alias = owner.Get(); escaped = alias.Data; }
 return escaped[0]; } }''', 'borrow'),
        'escape_array_initializer': ('''class Program { public static int Main() {
 STRONG<ArrayPayload> owner = SharedPointers.MAKE<ArrayPayload>(); borrowed ArrayPayload alias = owner.Get();
 int[] escaped = alias.Data; return escaped[0]; } }''', 'borrow'),
        'escape_array_return': ('''class Program { public static int[] Leak() {
 STRONG<ArrayPayload> owner = SharedPointers.MAKE<ArrayPayload>(); borrowed ArrayPayload alias = owner.Get();
 return alias.Data; } public static int Main() { return 0; } }''', 'borrow'),
        'pass_readonly_array': ('''class Program { public static void Mutate(int[] value) { value[0] = 10; }
 public static int Main() { STRONG<ArrayPayload> owner = SharedPointers.MAKE<ArrayPayload>();
 borrowed ArrayPayload alias = owner.Get(); Mutate(alias.Data); return 0; } }''', 'readonly borrowed'),
        'ref_readonly_array_element': ('''class Program { public static int Main() {
 STRONG<ArrayPayload> owner = SharedPointers.MAKE<ArrayPayload>(); borrowed ArrayPayload alias = owner.Get();
 ref int element = ref alias.Data[0]; element = 10; return 0; } }''', 'mutable addressable storage'),
        'mutate_class_in_array': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>();
 borrowed ChildArrayPayload alias = owner.Get(); alias.Data[0].Value = 10; return 0; } }''', 'not assignable'),
        'direct_foreach_read': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>(); int total = 0;
 foreach (ref Child child in owner.Get().Data) { total = total + child.Value; }
 return total == 0 ? 42 : 1; } }''', 'run:42'),
        'direct_foreach_escape': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>(); Child saved;
 foreach (Child child in owner.Get().Data) { saved = child; } return 0; } }''', 'borrowed value cannot'),
        'direct_foreach_move': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>();
 foreach (Child child in owner.Get().Data) { STRONG<ChildArrayPayload> moved = move(owner); }
 return 0; } }''', 'cannot move owner'),
        'direct_view_foreach_read': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>(); int total = 0;
 foreach (ref Child child in owner.Borrow().Get().Data) { total = total + child.Value; }
 return total == 0 ? 42 : 1; } }''', 'run:42'),
        'direct_view_foreach_escape': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>(); Child saved;
 foreach (Child child in owner.Borrow().Get().Data) { saved = child; } return 0; } }''', 'borrowed value cannot'),
        'direct_view_foreach_move': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>();
 foreach (Child child in owner.Borrow().Get().Data) { STRONG<ChildArrayPayload> moved = move(owner); }
 return 0; } }''', 'cannot move owner'),
        'escape_class_in_array': ('''class Program { public static int Main() {
 STRONG<ChildArrayPayload> owner = SharedPointers.MAKE<ChildArrayPayload>();
 borrowed ChildArrayPayload alias = owner.Get(); Child escaped = alias.Data[0];
 return escaped.Value; } }''', 'borrowed value cannot be stored in unqualified local'),
        'array_read_control': ('''class Program { public static int Main() {
 STRONG<ArrayPayload> owner = SharedPointers.MAKE<ArrayPayload>(); borrowed ArrayPayload alias = owner.Get();
 ArrayPayload ordinary = new ArrayPayload(); ordinary.Data[0] = 42;
 return alias.Data[0] == 0 && ordinary.Data[0] == 42 ? 42 : 1; } }''', 'run:42'),
        'move': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = owner.Borrow();
 STRONG<Payload> other = move(owner); return (int)view.Get().Data; } }''', 'cannot move owner'),
        'overwrite_live_view': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = owner.Borrow();
 owner = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite owner'),
        'overwrite_field_live_view': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.value.Borrow();
 holder.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_alias_field_live_view': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder(); borrowed Holder alias = holder;
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.value.Borrow();
 alias.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_reassigned_view_field': ('''class Holder { public STRONG<Payload> first; public STRONG<Payload> second; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.first = SharedPointers.MAKE<Payload>(); holder.second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.first.Borrow(); view = holder.second.Borrow();
 holder.second = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_reassigned_alias_field': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); second.value = SharedPointers.MAKE<Payload>();
 borrowed Holder alias = first; alias = second; SharedView<Payload> view = alias.value.Borrow();
 second.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_possible_parameter_alias': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Work(borrowed Holder first, borrowed Holder second) {
 SharedView<Payload> view = first.value.Borrow(); second.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } public static int Main() { return 0; } }''', 'cannot overwrite'),
        'overwrite_field_via_method': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.value.Borrow();
 holder.Replace(); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_field_via_borrowed_argument': ('''class Holder { public STRONG<Payload> value; }
class Program { public static void Replace(borrowed Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); Replace(holder); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_field_via_value_argument': ('''class Holder { public STRONG<Payload> value; }
class Program { public static void Replace(Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); Replace(holder); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_field_via_alias_argument': ('''class Holder { public STRONG<Payload> value; }
class Program { public static void Replace(borrowed Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 borrowed Holder alias = holder; SharedView<Payload> view = holder.value.Borrow();
 Replace(alias); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_field_via_named_argument': ('''class Holder { public STRONG<Payload> value; }
class Program { public static void Replace(int ignored, borrowed Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); Replace(item: holder, ignored: 1);
 return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_field_via_delegate_argument': ('''class Holder { public STRONG<Payload> value; }
delegate void Action(Holder item);
class Program { public static void Replace(Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Action action = Replace; Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.value.Borrow();
 action(holder); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_field_via_bound_delegate': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
delegate void Action();
class Program { public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 Action action = holder.Replace; SharedView<Payload> view = holder.value.Borrow();
 action(); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_field_via_late_bound_delegate': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
delegate void Action();
class Program { public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); Action action = holder.Replace;
 action(); return (int)view.Get().Data; } }''', 'borrow'),
        'bound_delegate_receiver_shadowed': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
delegate void Action();
class Program { public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 Action action = holder.Replace; SharedView<Payload> view = holder.value.Borrow();
 { Holder holder = new Holder(); action(); } return (int)view.Get().Data; } }''', 'borrow'),
        'bound_delegate_independent_holder': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
delegate void Action();
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); Action action = second.Replace;
 SharedView<Payload> view = first.value.Borrow(); action(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'bound_delegate_after_view_scope': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
delegate void Action();
class Program { public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 Action action = holder.Replace; { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } action(); return holder.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_field_via_event_argument': ('''class Holder { public STRONG<Payload> value; }
delegate void Action(Holder item);
class Program { public event Action Changed;
 public static void Replace(Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public int Work() { Changed += Replace; Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.value.Borrow(); Changed(holder); return (int)view.Get().Data; }
 public static int Main() { return 0; } }''', 'borrow'),
        'event_argument_independent_holder': ('''class Holder { public STRONG<Payload> value; }
delegate void Action(Holder item);
class Program { public event Action Changed;
 public static void Replace(Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public int Work() { Changed += Replace; Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 Changed(second); return view.Get().Data == 42l ? 42 : 1; }
 public static int Main() { Program program = new Program(); return program.Work(); } }''', 'run:42'),
        'event_argument_after_view_scope': ('''class Holder { public STRONG<Payload> value; }
delegate void Action(Holder item);
class Program { public event Action Changed;
 public static void Replace(Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public int Work() { Changed += Replace; Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = holder.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 Changed(holder); return holder.value.Get().Data == 42l ? 42 : 2; }
 public static int Main() { Program program = new Program(); return program.Work(); } }''', 'run:42'),
        'delegate_argument_independent_holder': ('''class Holder { public STRONG<Payload> value; }
delegate void Action(Holder item);
class Program { public static void Replace(Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Action action = Replace; Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 action(second); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'delegate_argument_after_view_scope': ('''class Holder { public STRONG<Payload> value; }
delegate void Action(Holder item);
class Program { public static void Replace(Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Action action = Replace; Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } action(holder); return holder.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'argument_independent_holder': ('''class Holder { public STRONG<Payload> value; }
class Program { public static void Replace(borrowed Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 Replace(second); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'argument_after_view_scope': ('''class Holder { public STRONG<Payload> value; }
class Program { public static void Replace(borrowed Holder item) { item.value = SharedPointers.MAKE<Payload>(); }
 public static int Main() { Holder holder = new Holder(); holder.value = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = holder.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 Replace(holder); return holder.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_this_field_via_implicit_method': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); }
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = value.Borrow();
 Replace(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'borrow'),
        'overwrite_this_field_via_explicit_method': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); }
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = this.value.Borrow();
 this.Replace(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'borrow'),
        'implicit_method_after_this_view_scope': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); }
 public int Work() { value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = value.Borrow();
 if (view.Get().Data != 42l) return 1; } Replace(); return value.Get().Data == 42l ? 42 : 2; } }
class Program { public static int Main() { Holder holder = new Holder(); return holder.Work(); } }''', 'run:42'),
        'static_method_during_this_view': ('''class Holder { public STRONG<Payload> value;
 public static int Read() { return 42; }
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = value.Borrow();
 return Read() == 42 && view.Get().Data == 42l ? 42 : 1; } }
class Program { public static int Main() { Holder holder = new Holder(); return holder.Work(); } }''', 'run:42'),
        'overwrite_field_via_alias_method': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); borrowed Holder alias = holder;
 SharedView<Payload> view = holder.value.Borrow(); alias.Replace(); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_nested_field_via_method': ('''class Inner { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.value.Borrow();
 holder.child.Replace(); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_nested_struct_via_method': ('''struct Inner { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Holder { public Inner child; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.value.Borrow();
 holder.child.Replace(); return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_indexed_receiver_via_method': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Program { public static int Main() { Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holders[0].value.Borrow();
 holders[0].Replace(); return (int)view.Get().Data; } }''', 'borrow'),
        'method_on_independent_indexed_receiver': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Program { public static int Main() { Holder[] first = new Holder[1]; Holder[] second = new Holder[1];
 first[0] = new Holder(); second[0] = new Holder(); first[0].value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = first[0].value.Borrow(); second[0].Replace();
 return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'method_on_other_index_conservative': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Program { public static int Main() { Holder[] holders = new Holder[2];
 holders[0] = new Holder(); holders[1] = new Holder(); holders[0].value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holders[0].value.Borrow(); holders[1].Replace();
 return (int)view.Get().Data; } }''', 'borrow'),
        'indexed_method_after_view_scope': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Program { public static int Main() { Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holders[0].value.Borrow();
 if (view.Get().Data != 42l) return 1; } holders[0].Replace();
 return holders[0].value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'method_on_independent_nested_struct': ('''struct Inner { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Holder { public Inner child; }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.child.value.Borrow();
 second.child.Replace(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'method_on_independent_nested_holder': ('''class Inner { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.child.value.Borrow();
 second.child.Replace(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'readonly_nested_owner_with_view': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.value.Borrow();
 return holder.child.value.Get().Data == 42l && view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'method_on_independent_holder': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.value.Borrow();
 second.Replace(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'method_after_field_view_scope': ('''class Holder { public STRONG<Payload> value;
 public void Replace() { value = SharedPointers.MAKE<Payload>(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } holder.Replace();
 return holder.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_union_view': ('''union Holder { public STRONG<Payload> first; public STRONG<Payload> second; }
class Program { public static int Main() { Holder holder; holder.first = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.first.Borrow(); holder.second = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_explicit_overlap_view': ('''struct Holder {
 public at(0) STRONG<Payload> first; public at(0) STRONG<Payload> second; }
class Program { public static int Main() { Holder holder; holder.first = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.first.Borrow(); holder.second = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'borrow'),
        'overwrite_explicit_disjoint_view': ('''struct Holder {
 public at(0) STRONG<Payload> first; public at(64) STRONG<Payload> second; }
class Program { public static int Main() { Holder holder; holder.first = SharedPointers.MAKE<Payload>();
 holder.second = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.first.Borrow();
 holder.second = SharedPointers.MAKE<Payload>(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'explicit_disjoint_without_view': ('''struct Holder {
 public at(0) STRONG<Payload> first; public at(64) STRONG<Payload> second; }
class Program { public static int Main() { Holder holder; holder.first = SharedPointers.MAKE<Payload>();
 holder.second = SharedPointers.MAKE<Payload>(); return holder.first.Get().Data == 42l ? 42 : 1; } }''',
                                           'run:42'),
        'union_without_view': ('''union Holder { public STRONG<Payload> first; public STRONG<Payload> second; }
class Program { public static int Main() { Holder holder; holder.first = SharedPointers.MAKE<Payload>();
 return holder.first.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'overwrite_nested_field_view': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.value.Borrow();
 holder.child.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_field_view_root': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); Holder replacement = new Holder();
 SharedView<Payload> view = holder.value.Borrow(); holder = move(replacement);
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_field_view_root_after_scope': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } Holder replacement = new Holder(); holder = move(replacement);
 holder.value = SharedPointers.MAKE<Payload>(); return holder.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'move_field_view_root': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.value.Borrow();
 owned Holder moved = move(holder); return (int)view.Get().Data; } }''', 'cannot move'),
        'move_borrowed_owner_field': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.value.Borrow();
 { STRONG<Payload> moved = move(holder.value); } return (int)view.Get().Data; } }''', 'cannot partially move'),
        'move_borrowed_owner_field_alias': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); Inner alias = holder.child;
 SharedView<Payload> view = holder.child.value.Borrow();
 { STRONG<Payload> moved = move(alias.value); } return (int)view.Get().Data; } }''', 'cannot partially move'),
        'move_borrowed_this_field': ('''class Holder { public STRONG<Payload> value;
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = this.value.Borrow();
 { STRONG<Payload> moved = move(this.value); } return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot partially move'),
        'move_borrowed_implicit_field': ('''class Holder { public STRONG<Payload> value;
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = value.Borrow();
 { STRONG<Payload> moved = move(value); } return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot partially move'),
        'move_borrowed_implicit_nested_field': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = child.value.Borrow();
 { STRONG<Payload> moved = move(child.value); } return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot partially move'),
        'move_borrowed_array_element': ('''class Program { public static int Main() {
 STRONG<Payload>[] values = new STRONG<Payload>[1]; values[0] = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = values[0].Borrow(); { STRONG<Payload> moved = move(values[0]); }
 return (int)view.Get().Data; } }''', 'cannot partially move'),
        'move_borrowed_implicit_array_field': ('''class Holder { public STRONG<Payload>[] items;
 public Holder() { items = new STRONG<Payload>[1]; }
 public int Work() { items[0] = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = items[0].Borrow();
 { STRONG<Payload> moved = move(items[0]); } return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot partially move'),
        'move_borrowed_explicit_array_field': ('''class Holder { public STRONG<Payload>[] items;
 public Holder() { items = new STRONG<Payload>[1]; }
 public int Work() { items[0] = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = this.items[0].Borrow();
 { STRONG<Payload> moved = move(this.items[0]); } return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot partially move'),
        'move_array_field_after_view': ('''class Holder { public STRONG<Payload>[] items;
 public Holder() { items = new STRONG<Payload>[1]; }
 public int Work() { items[0] = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = items[0].Borrow(); if (view.Get().Data != 42l) return 1; }
 STRONG<Payload> moved = move(items[0]); return moved.Get().Data == 42l ? 42 : 2; } }
class Program { public static int Main() { Holder holder = new Holder(); return holder.Work(); } }''', 'run:42'),
        'move_array_element_after_view': ('''class Program { public static int Main() {
 STRONG<Payload>[] values = new STRONG<Payload>[1]; values[0] = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = values[0].Borrow(); if (view.Get().Data != 42l) return 1; }
 STRONG<Payload> moved = move(values[0]); return moved.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'move_independent_alias_field': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.child.value = SharedPointers.MAKE<Payload>(); second.child.value = SharedPointers.MAKE<Payload>();
 Inner alias = second.child; SharedView<Payload> view = first.child.value.Borrow();
 STRONG<Payload> moved = move(alias.value); return moved.Get().Data == 42l && view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'move_other_field_through_alias': ('''class Inner { public STRONG<Payload> first; public STRONG<Payload> second; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.first = SharedPointers.MAKE<Payload>(); holder.child.second = SharedPointers.MAKE<Payload>();
 Inner alias = holder.child; SharedView<Payload> view = holder.child.first.Borrow();
 STRONG<Payload> moved = move(alias.second); return moved.Get().Data == 42l && view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'move_owner_field_after_view': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } STRONG<Payload> moved = move(holder.value);
 return moved.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'move_field_view_root_after_scope': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } owned Holder moved = move(holder);
 return moved.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_nested_owner_view': ('''class Inner { public STRONG<Payload> value; }
class Holder { public owned Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.value.Borrow();
 holder.child = new Inner(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_other_nested_owner': ('''class Inner { public STRONG<Payload> value; }
class Holder { public owned Inner child; public owned Inner other;
 public Holder() { child = new Inner(); other = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.value.Borrow();
 holder.other = new Inner(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'overwrite_nested_owner_after_view': ('''class Inner { public STRONG<Payload> value; }
class Holder { public owned Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.child.value.Borrow();
 if (view.Get().Data != 42l) return 1; } holder.child = new Inner(); return 42; } }''', 'run:42'),
        'overwrite_indexed_field_view': ('''class Holder { public STRONG<Payload>[] values;
 public Holder() { values = new STRONG<Payload>[1]; } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.values[0] = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.values[0].Borrow();
 holder.values[0] = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_local_array_view': ('''class Program { public static int Main() {
 STRONG<Payload>[] values = new STRONG<Payload>[1]; values[0] = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = values[0].Borrow(); values[0] = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_local_array_after_view': ('''class Program { public static int Main() {
 STRONG<Payload>[] values = new STRONG<Payload>[1]; values[0] = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = values[0].Borrow(); if (view.Get().Data != 42l) return 1; }
 values[0] = SharedPointers.MAKE<Payload>(); return values[0].Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_chained_view_field': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> first = holder.value.Borrow();
 SharedView<Payload> second = first; holder.value = SharedPointers.MAKE<Payload>();
 return (int)second.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_field_method_view': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.View();
 holder.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_field_method_view_alias': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); borrowed Holder alias = holder;
 SharedView<Payload> view = holder.View(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'method_view_parameter_alias': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Work(borrowed Holder first, borrowed Holder second) {
 SharedView<Payload> view = first.View(); second.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } public static int Main() { return 0; } }''', 'cannot overwrite'),
        'method_view_other_receiver': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); second.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = first.View(); second.value = SharedPointers.MAKE<Payload>();
 return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'method_view_other_field_conservative': ('''class Holder { public STRONG<Payload> value;
 public STRONG<Payload> other; public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); holder.other = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.View(); holder.other = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_nested_method_view': ('''class Inner { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.child.View();
 holder.child.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'nested_method_view_after_scope': ('''class Inner { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.child.View();
 if (view.Get().Data != 42l) return 1; } holder.child.value = SharedPointers.MAKE<Payload>();
 return holder.child.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_indexed_method_view': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder[] holders = new Holder[1];
 holders[0] = new Holder(); holders[0].value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holders[0].View(); holders[0].value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'indexed_method_view_after_scope': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder[] holders = new Holder[1];
 holders[0] = new Holder(); holders[0].value = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = holders[0].View(); if (view.Get().Data != 42l) return 1; }
 holders[0].value = SharedPointers.MAKE<Payload>(); return holders[0].value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_indexed_field_method_view': ('''class Inner { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Holder { public Inner[] items; public Holder() { items = new Inner[1]; items[0] = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.items[0].value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.items[0].View();
 holder.items[0].value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'method_view_class_alias_mutation': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.View();
 borrowed Payload alias = view.Get(); alias.Data = 7l; return (int)alias.Data; } }''', 'not assignable'),
        'method_view_direct_class_mutation': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); holder.View().Get().Data = 7l; return 0; } }''', 'not assignable'),
        'method_view_return_class': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static borrowed Payload Leak() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = holder.View();
 return view.Get(); } public static int Main() { return 0; } }''', 'cannot be returned'),
        'overwrite_this_field_view': ('''class Holder { public STRONG<Payload> value;
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = this.value.Borrow();
 this.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'overwrite_implicit_nested_this_view': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = child.value.Borrow(); child.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'implicit_nested_this_after_scope': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = child.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 child.value = SharedPointers.MAKE<Payload>(); return child.value.Get().Data == 42l ? 42 : 2; } }
class Program { public static int Main() { Holder holder = new Holder(); return holder.Work(); } }''', 'run:42'),
        'implicit_nested_explicit_target': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = child.value.Borrow(); this.child.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'explicit_nested_implicit_target': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = this.child.value.Borrow(); child.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'implicit_nested_intermediate_owner': ('''class Inner { public STRONG<Payload> value; }
class Holder { public owned Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = child.value.Borrow(); child = new Inner(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'implicit_nested_class_alias': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>(); Inner alias = child;
 SharedView<Payload> view = child.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'object_field_class_alias': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); Inner alias = holder.child;
 SharedView<Payload> view = holder.child.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'array_element_class_alias': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Payload>(); Holder alias = holders[0];
 SharedView<Payload> view = holders[0].value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'array_element_class_alias_reverse': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Payload>(); Holder alias = holders[0];
 SharedView<Payload> view = alias.value.Borrow(); holders[0].value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'array_element_class_alias_conditional': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder[] first = new Holder[1]; Holder[] second = new Holder[1];
 first[0] = new Holder(); second[0] = new Holder(); first[0].value = SharedPointers.MAKE<Payload>();
 bool choose = true; Holder alias = choose ? first[0] : second[0];
 SharedView<Payload> view = first[0].value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'array_element_alias_after_scope': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder[] holders = new Holder[1]; holders[0] = new Holder();
 holders[0].value = SharedPointers.MAKE<Payload>(); Holder alias = holders[0];
 { SharedView<Payload> view = holders[0].value.Borrow(); if (view.Get().Data != 42l) return 1; }
 alias.value = SharedPointers.MAKE<Payload>(); return alias.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'array_element_alias_assignment': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder[] first = new Holder[1]; Holder[] second = new Holder[1];
 first[0] = new Holder(); second[0] = new Holder(); first[0].value = SharedPointers.MAKE<Payload>();
 Holder alias = second[0]; alias = first[0]; SharedView<Payload> view = first[0].value.Borrow();
 alias.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'array_element_alias_independent': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder[] first = new Holder[1]; Holder[] second = new Holder[1];
 first[0] = new Holder(); second[0] = new Holder(); first[0].value = SharedPointers.MAKE<Payload>();
 Holder alias = second[0]; SharedView<Payload> view = first[0].value.Borrow();
 alias.value = SharedPointers.MAKE<Payload>(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'array_field_class_alias': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner[] items; public Holder() { items = new Inner[1]; items[0] = new Inner(); }
 public int Work() { items[0].value = SharedPointers.MAKE<Payload>(); Inner alias = items[0];
 SharedView<Payload> view = items[0].value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'array_parameter_class_alias': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Work(Holder[] items) { Holder[] separate = new Holder[1]; separate[0] = new Holder();
 items[0].value = SharedPointers.MAKE<Payload>(); Holder alias = separate[0];
 alias = items[0]; SharedView<Payload> view = items[0].value.Borrow();
 alias.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; }
 public static int Main() { return 0; } }''', 'cannot overwrite'),
        'array_container_alias_chain': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Work(Holder[] holders) { holders[0].value = SharedPointers.MAKE<Payload>();
 Holder[] aliasArray = holders; Holder alias = aliasArray[0];
 SharedView<Payload> view = holders[0].value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } public static int Main() { return 0; } }''', 'cannot overwrite'),
        'array_container_alias_assignment': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Work(Holder[] holders) { Holder[] separate = new Holder[1];
 separate[0] = new Holder(); holders[0].value = SharedPointers.MAKE<Payload>();
 Holder[] aliasArray = separate; aliasArray = holders; Holder alias = aliasArray[0];
 SharedView<Payload> view = holders[0].value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } public static int Main() { return 0; } }''', 'cannot overwrite'),
        'object_field_class_alias_after_scope': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); Inner alias = holder.child;
 { SharedView<Payload> view = holder.child.value.Borrow(); if (view.Get().Data != 42l) return 1; }
 alias.value = SharedPointers.MAKE<Payload>(); return alias.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'object_field_alias_assignment': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Work(Holder holder, Inner other) {
 holder.child.value = SharedPointers.MAKE<Payload>(); Inner alias = other; alias = holder.child;
 SharedView<Payload> view = holder.child.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } public static int Main() { return 0; } }''', 'cannot overwrite'),
        'object_field_alias_local_assignment': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.child.value = SharedPointers.MAKE<Payload>(); Inner alias = second.child; alias = first.child;
 SharedView<Payload> view = first.child.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'parameter_field_alias_reassignment': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Work(Holder holder) { Holder other = new Holder();
 holder.child.value = SharedPointers.MAKE<Payload>(); Inner alias = other.child; alias = holder.child;
 SharedView<Payload> view = holder.child.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } public static int Main() { return 0; } }''', 'cannot overwrite'),
        'parameter_field_alias_conditional': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Work(Holder holder) { Holder other = new Holder(); bool choose = true;
 holder.child.value = SharedPointers.MAKE<Payload>(); Inner alias = other.child;
 alias = choose ? holder.child : other.child; SharedView<Payload> view = holder.child.value.Borrow();
 alias.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; }
 public static int Main() { return 0; } }''', 'cannot overwrite'),
        'object_field_other_holder_control': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); } }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.child.value = SharedPointers.MAKE<Payload>(); second.child.value = SharedPointers.MAKE<Payload>();
 Inner alias = second.child; SharedView<Payload> view = first.child.value.Borrow();
 alias.value = SharedPointers.MAKE<Payload>(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'explicit_nested_class_alias': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>(); Inner alias = this.child;
 SharedView<Payload> view = child.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'nested_class_alias_reassigned': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work(Inner other) { child.value = SharedPointers.MAKE<Payload>(); Inner alias = other; alias = child;
 SharedView<Payload> view = child.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'nested_class_alias_chain': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>(); Inner first = child; Inner second = first;
 SharedView<Payload> view = child.value.Borrow(); second.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'nested_class_alias_assignment_chain': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work(Inner other) { child.value = SharedPointers.MAKE<Payload>();
 Inner first = child; Inner second = other; second = first; SharedView<Payload> view = child.value.Borrow();
 second.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'nested_class_alias_branch_cycle': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work(Inner other) { child.value = SharedPointers.MAKE<Payload>();
 Inner first = other; Inner second = other; bool choose = true;
 if (choose) { first = child; } second = first; first = second;
 SharedView<Payload> view = child.value.Borrow(); second.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'nested_class_alias_conditional_assignment': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work(Inner other) { child.value = SharedPointers.MAKE<Payload>();
 Inner first = child; Inner second = other; bool choose = true; second = choose ? first : other;
 SharedView<Payload> view = child.value.Borrow(); second.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'nested_class_alias_conditional_initializer': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work(Inner other) { child.value = SharedPointers.MAKE<Payload>();
 bool choose = false; Inner alias = choose ? other : child;
 SharedView<Payload> view = child.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'independent_nested_class_control': ('''class Inner { public STRONG<Payload> value; }
class Holder { public Inner child; public Holder() { child = new Inner(); }
 public int Work() { child.value = SharedPointers.MAKE<Payload>(); Inner other = new Inner();
 other.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = child.value.Borrow();
 other.value = SharedPointers.MAKE<Payload>(); return view.Get().Data == 42l ? 42 : 1; } }
class Program { public static int Main() { Holder holder = new Holder(); return holder.Work(); } }''', 'run:42'),
        'overwrite_this_method_view': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); }
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = this.View();
 this.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'overwrite_this_via_alias': ('''class Holder { public STRONG<Payload> value;
 public int Work() { value = SharedPointers.MAKE<Payload>(); borrowed Holder alias = this;
 SharedView<Payload> view = this.value.Borrow(); alias.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'borrowed local requires'),
        'overwrite_implicit_this_view': ('''class Holder { public STRONG<Payload> value;
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = value.Borrow();
 value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'overwrite_implicit_view_explicit_target': ('''class Holder { public STRONG<Payload> value;
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = value.Borrow();
 this.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'overwrite_explicit_view_implicit_target': ('''class Holder { public STRONG<Payload> value;
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = this.value.Borrow();
 value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'overwrite_this_view_parameter_alias': ('''class Holder { public STRONG<Payload> value;
 public int Work(borrowed Holder other) { value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = this.value.Borrow(); other.value = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } } class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'overwrite_parameter_view_this_alias': ('''class Holder { public STRONG<Payload> value;
 public int Work(borrowed Holder other) { SharedView<Payload> view = other.value.Borrow();
 this.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'overwrite_implicit_method_view': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); }
 public int Work() { value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = View();
 this.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }
class Program { public static int Main() { return 0; } }''', 'cannot overwrite'),
        'this_view_after_scope': ('''class Holder { public STRONG<Payload> value;
 public int Work() { value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = this.value.Borrow();
 if (view.Get().Data != 42l) return 1; } this.value = SharedPointers.MAKE<Payload>();
 return this.value.Get().Data == 42l ? 42 : 2; } }
class Program { public static int Main() { Holder holder = new Holder(); return holder.Work(); } }''', 'run:42'),
        'this_view_other_field': ('''class Holder { public STRONG<Payload> first; public STRONG<Payload> second;
 public int Work() { first = SharedPointers.MAKE<Payload>(); second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = this.first.Borrow(); this.second = SharedPointers.MAKE<Payload>();
 return view.Get().Data == 42l ? 42 : 1; } }
class Program { public static int Main() { Holder holder = new Holder(); return holder.Work(); } }''', 'run:42'),
        'method_view_after_scope': ('''class Holder { public STRONG<Payload> value;
 public SharedView<Payload> View() { return value.Borrow(); } }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.View();
 if (view.Get().Data != 42l) return 1; } holder.value = SharedPointers.MAKE<Payload>();
 return holder.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_other_field_view': ('''class Holder { public STRONG<Payload> first; public STRONG<Payload> second; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.first = SharedPointers.MAKE<Payload>(); holder.second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.first.Borrow(); holder.second = SharedPointers.MAKE<Payload>();
 return view.Get().Data == 42l && holder.second.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'overwrite_conditional_view_first': ('''class Holder { public STRONG<Payload> first; public STRONG<Payload> second; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.first = SharedPointers.MAKE<Payload>(); holder.second = SharedPointers.MAKE<Payload>();
 bool choose = true; SharedView<Payload> view = choose ? holder.first.Borrow() : holder.second.Borrow();
 holder.first = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_conditional_owner_second': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 bool choose = false; SharedView<Payload> view = choose ? first.Borrow() : second.Borrow();
 second = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_conditional_parameter_second': ('''class Program {
 public static int Work(STRONG<Payload> first, STRONG<Payload> second) {
 bool choose = false; SharedView<Payload> view = choose ? first.Borrow() : second.Borrow();
 second = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>(), SharedPointers.MAKE<Payload>()); } }''',
                                                    'cannot overwrite'),
        'conditional_owner_inner_scope_escape': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = first.Borrow();
 { STRONG<Payload> second = SharedPointers.MAKE<Payload>(); bool choose = false;
 view = choose ? first.Borrow() : second.Borrow(); } return (int)view.Get().Data; } }''',
                                                  'cannot be stored in a longer-lived local'),
        'conditional_field_inner_scope_escape': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder first = new Holder(); first.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = first.value.Borrow(); { Holder second = new Holder();
 second.value = SharedPointers.MAKE<Payload>(); bool choose = false;
 view = choose ? first.value.Borrow() : second.value.Borrow(); } return (int)view.Get().Data; } }''',
                                                  'cannot be stored in a longer-lived local'),
        'conditional_reassign_same_scope': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = first.Borrow(); bool choose = false;
 view = choose ? first.Borrow() : second.Borrow(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'conditional_inner_scope_read': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); { STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = first.Borrow(); bool choose = false;
 view = choose ? first.Borrow() : second.Borrow(); return view.Get().Data == 42l ? 42 : 1; } } }''', 'run:42'),
        'conditional_view_alias_owner_escape': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); SharedView<Payload> result = first.Borrow();
 { STRONG<Payload> second = SharedPointers.MAKE<Payload>(); SharedView<Payload> alias = second.Borrow();
 bool choose = false; result = choose ? first.Borrow() : alias; } return (int)result.Get().Data; } }''',
                                                 'cannot be stored in a longer-lived local'),
        'conditional_view_alias_owner_overwrite': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> alias = second.Borrow(); bool choose = false;
 SharedView<Payload> result = choose ? first.Borrow() : alias;
 second = SharedPointers.MAKE<Payload>(); return (int)result.Get().Data; } }''', 'cannot overwrite'),
        'conditional_view_alias_read': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> alias = second.Borrow(); bool choose = false;
 SharedView<Payload> result = choose ? first.Borrow() : alias; return result.Get().Data == 42l ? 42 : 1; } }''',
                                       'run:42'),
        'conditional_expired_alias_owner_overwrite': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> result = first.Borrow(); { SharedView<Payload> alias = second.Borrow(); bool choose = false;
 result = choose ? first.Borrow() : alias; } second = SharedPointers.MAKE<Payload>();
 return (int)result.Get().Data; } }''', 'cannot overwrite'),
        'conditional_expired_alias_read': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> result = first.Borrow(); { SharedView<Payload> alias = second.Borrow(); bool choose = false;
 result = choose ? first.Borrow() : alias; } return result.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'shadow_unrelated_owner_overwrite': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = owner.Borrow();
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); owner = SharedPointers.MAKE<Payload>(); }
 return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'shadow_unrelated_owner_move': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = owner.Borrow();
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); STRONG<Payload> moved = move(owner); }
 return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'shadow_inner_owner_borrowed': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = owner.Borrow(); owner = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; }
 } }''', 'cannot overwrite'),
        'shadow_outer_owner_borrowed': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = owner.Borrow();
 { STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); } owner = SharedPointers.MAKE<Payload>();
 return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'shadow_unrelated_owner_field': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder owner = new Holder(); owner.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = owner.value.Borrow(); { Holder owner = new Holder();
 owner.value = SharedPointers.MAKE<Payload>(); owner.value = SharedPointers.MAKE<Payload>(); }
 return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'shadow_inner_owner_field_borrowed': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder owner = new Holder(); { Holder owner = new Holder();
 owner.value = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = owner.value.Borrow();
 owner.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } } }''', 'cannot overwrite'),
        'shadow_outer_owner_field_borrowed': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder owner = new Holder(); owner.value = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = owner.value.Borrow(); { Holder owner = new Holder(); }
 owner.value = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'move_conditional_parameter_second': ('''class Program {
 public static int Work(STRONG<Payload> first, STRONG<Payload> second) {
 bool choose = false; SharedView<Payload> view = choose ? first.Borrow() : second.Borrow();
 STRONG<Payload> moved = move(second); return (int)view.Get().Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>(), SharedPointers.MAKE<Payload>()); } }''',
                                               'cannot move'),
        'overwrite_conditional_other_owner': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 STRONG<Payload> other = SharedPointers.MAKE<Payload>(); bool choose = false;
 SharedView<Payload> view = choose ? first.Borrow() : second.Borrow();
 other = SharedPointers.MAKE<Payload>(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'overwrite_conditional_parameter_after_scope': ('''class Program {
 public static int Work(STRONG<Payload> first, STRONG<Payload> second) {
 { bool choose = false; SharedView<Payload> view = choose ? first.Borrow() : second.Borrow();
 if (view.Get().Data != 42l) return 1; } second = SharedPointers.MAKE<Payload>();
 return second.Get().Data == 42l ? 42 : 2; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>(), SharedPointers.MAKE<Payload>()); } }''',
                                                       'run:42'),
        'overwrite_conditional_view_second': ('''class Holder { public STRONG<Payload> first; public STRONG<Payload> second; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.first = SharedPointers.MAKE<Payload>(); holder.second = SharedPointers.MAKE<Payload>();
 bool choose = false; SharedView<Payload> view = choose ? holder.first.Borrow() : holder.second.Borrow();
 holder.second = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_conditional_view_reassigned': ('''class Holder { public STRONG<Payload> first; public STRONG<Payload> second; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.first = SharedPointers.MAKE<Payload>(); holder.second = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = holder.first.Borrow(); bool choose = false;
 view = choose ? holder.first.Borrow() : holder.second.Borrow();
 holder.second = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; } }''', 'cannot overwrite'),
        'overwrite_conditional_second_root': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); second.value = SharedPointers.MAKE<Payload>();
 bool choose = false; SharedView<Payload> view = choose ? first.value.Borrow() : second.value.Borrow();
 Holder replacement = new Holder(); second = move(replacement); return (int)view.Get().Data; } }''',
                                               'cannot overwrite'),
        'move_conditional_second_root': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); second.value = SharedPointers.MAKE<Payload>();
 bool choose = false; SharedView<Payload> view = choose ? first.value.Borrow() : second.value.Borrow();
 owned Holder moved = move(second); return (int)view.Get().Data; } }''', 'cannot move'),
        'overwrite_conditional_first_root': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); second.value = SharedPointers.MAKE<Payload>();
 bool choose = true; SharedView<Payload> view = choose ? first.value.Borrow() : second.value.Borrow();
 Holder replacement = new Holder(); first = move(replacement); return (int)view.Get().Data; } }''',
                                              'cannot overwrite'),
        'overwrite_conditional_other_root': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 Holder other = new Holder(); first.value = SharedPointers.MAKE<Payload>();
 second.value = SharedPointers.MAKE<Payload>(); bool choose = false;
 SharedView<Payload> view = choose ? first.value.Borrow() : second.value.Borrow();
 Holder replacement = new Holder(); other = move(replacement); return view.Get().Data == 42l ? 42 : 1; } }''',
                                              'run:42'),
        'overwrite_conditional_root_after_scope': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder first = new Holder(); Holder second = new Holder();
 first.value = SharedPointers.MAKE<Payload>(); second.value = SharedPointers.MAKE<Payload>();
 { bool choose = false; SharedView<Payload> view = choose ? first.value.Borrow() : second.value.Borrow();
 if (view.Get().Data != 42l) return 1; } Holder replacement = new Holder(); second = move(replacement);
 second.value = SharedPointers.MAKE<Payload>(); return second.value.Get().Data == 42l ? 42 : 2; } }''',
                                                    'run:42'),
        'overwrite_conditional_view_other': ('''class Holder { public STRONG<Payload> first; public STRONG<Payload> second;
 public STRONG<Payload> third; } class Program { public static int Main() { Holder holder = new Holder();
 holder.first = SharedPointers.MAKE<Payload>(); holder.second = SharedPointers.MAKE<Payload>();
 holder.third = SharedPointers.MAKE<Payload>(); bool choose = true;
 SharedView<Payload> view = choose ? holder.first.Borrow() : holder.second.Borrow();
 holder.third = SharedPointers.MAKE<Payload>(); return view.Get().Data == 42l ? 42 : 1; } }''', 'run:42'),
        'overwrite_conditional_view_after_scope': ('''class Holder { public STRONG<Payload> first; public STRONG<Payload> second; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.first = SharedPointers.MAKE<Payload>(); holder.second = SharedPointers.MAKE<Payload>();
 { bool choose = false; SharedView<Payload> view = choose ? holder.first.Borrow() : holder.second.Borrow();
 if (view.Get().Data != 42l) return 1; } holder.second = SharedPointers.MAKE<Payload>();
 return holder.second.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_field_after_view': ('''class Holder { public STRONG<Payload> value; }
class Program { public static int Main() { Holder holder = new Holder();
 holder.value = SharedPointers.MAKE<Payload>(); { SharedView<Payload> view = holder.value.Borrow();
 if (view.Get().Data != 42l) return 1; } holder.value = SharedPointers.MAKE<Payload>();
 return holder.value.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'overwrite_live_get': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 owner = SharedPointers.MAKE<Payload>(); return (int)alias.Data; } }''', 'cannot overwrite owner'),
        'conditional_get_owner_overwrite': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 bool choose = false; borrowed Payload alias = choose ? first.Get() : second.Get();
 second = SharedPointers.MAKE<Payload>(); return (int)alias.Data; } }''', 'borrowed local requires a direct live owner'),
        'conditional_get_alias_mutate': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 bool choose = false; borrowed Payload alias = choose ? first.Get() : second.Get(); alias.Data = 7l; return 0; } }''',
                                         'borrowed local requires a direct live owner'),
        'conditional_get_alias_read': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 bool choose = false; borrowed Payload alias = choose ? first.Get() : second.Get();
 return alias.Data == 42l ? 42 : 1; } }''', 'borrowed local requires a direct live owner'),
        'conditional_get_direct_mutate': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 bool choose = false; (choose ? first.Get() : second.Get()).Data = 7l; return 0; } }''', 'not assignable'),
        'conditional_get_direct_read': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 bool choose = false; return (choose ? first.Get() : second.Get()).Data == 42l ? 42 : 1; } }''', 'run:42'),
        'conditional_get_unqualified_escape': ('''class Program { public static int Main() {
 STRONG<Payload> first = SharedPointers.MAKE<Payload>(); STRONG<Payload> second = SharedPointers.MAKE<Payload>();
 bool choose = false; Payload alias = choose ? first.Get() : second.Get(); return (int)alias.Data; } }''',
                                              'borrowed value cannot be stored in unqualified local'),
        'overwrite_live_view_move': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); STRONG<Payload> replacement = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = owner.Borrow(); owner = move(replacement); return (int)view.Get().Data; } }''',
                                     'cannot overwrite owner'),
        'overwrite_parameter_live_view': ('''class Program {
 public static int Work(STRONG<Payload> owner) { SharedView<Payload> view = owner.Borrow();
 owner = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'cannot overwrite owner'),
        'move_parameter_live_view': ('''class Program {
 public static int Work(STRONG<Payload> owner) { SharedView<Payload> view = owner.Borrow();
 STRONG<Payload> other = move(owner); return (int)view.Get().Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'cannot move'),
        'overwrite_parameter_reassigned_view': ('''class Program {
 public static int Work(STRONG<Payload> owner) { SharedView<Payload> view; view = owner.Borrow();
 owner = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'cannot overwrite owner'),
        'return_parameter_view': ('''class Program {
 public static SharedView<Payload> Work(STRONG<Payload> owner) { return owner.Borrow(); }
 public static int Main() { return 0; } }''', 'cannot be returned'),
        'parameter_view_read': ('''class Program {
 public static int Work(STRONG<Payload> owner) { SharedView<Payload> view = owner.Borrow();
 return view.Get().Data == 42l ? 42 : 1; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'run:42'),
        'parameter_get_read': ('''class Program {
 public static int Work(STRONG<Payload> owner) { borrowed Payload alias = owner.Get();
 return alias.Data == 42l ? 42 : 1; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'run:42'),
        'parameter_struct_child_escape': ('''class Program {
 public static int Work(STRONG<ChildValue> owner) { Child escaped = owner.Get().Item;
 return escaped.Value; } public static int Main() { return 0; } }''', 'borrowed value cannot'),
        'parameter_struct_child_write': ('''class Program {
 public static int Work(STRONG<ChildValue> owner) { owner.Get().Item.Value = 9; return 0; }
 public static int Main() { return 0; } }''', 'not assignable'),
        'parameter_struct_read': ('''class Program {
 public static int Work(STRONG<ChildValue> owner) { return owner.Get().Number; }
 public static int Main() { return Work(SharedPointers.MAKE<ChildValue>()); } }''', 'run:42'),
        'parameter_struct_view_escape': ('''class Program {
 public static int Work(STRONG<ChildValue> owner) { SharedView<ChildValue> view = owner.Borrow();
 Child escaped = view.Get().Item; return escaped.Value; }
 public static int Main() { return 0; } }''', 'borrowed value cannot'),
        'parameter_struct_view_write': ('''class Program {
 public static int Work(STRONG<ChildValue> owner) { SharedView<ChildValue> view = owner.Borrow();
 view.Get().Item.Value = 9; return 0; } public static int Main() { return 0; } }''', 'not assignable'),
        'parameter_struct_view_read': ('''class Program {
 public static int Work(STRONG<ChildValue> owner) { SharedView<ChildValue> view = owner.Borrow();
 return view.Get().Number; } public static int Main() { return Work(SharedPointers.MAKE<ChildValue>()); } }''',
                                       'run:42'),
        'parameter_view_get_read': ('''class Program {
 public static int Work(STRONG<Payload> owner) { SharedView<Payload> view = owner.Borrow();
 borrowed Payload alias = view.Get(); return alias.Data == 42l ? 42 : 1; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'run:42'),
        'parameter_view_get_move': ('''class Program {
 public static int Work(STRONG<Payload> owner) { SharedView<Payload> view = owner.Borrow();
 borrowed Payload alias = view.Get(); STRONG<Payload> moved = move(owner); return (int)alias.Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'cannot move'),
        'parameter_view_get_overwrite': ('''class Program {
 public static int Work(STRONG<Payload> owner) { SharedView<Payload> view = owner.Borrow();
 borrowed Payload alias = view.Get(); owner = SharedPointers.MAKE<Payload>(); return (int)alias.Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'cannot overwrite owner'),
        'parameter_view_get_write': ('''class Program {
 public static int Work(STRONG<Payload> owner) { SharedView<Payload> view = owner.Borrow();
 borrowed Payload alias = view.Get(); alias.Data = 9l; return 0; }
 public static int Main() { return 0; } }''', 'not assignable'),
        'parameter_view_get_return': ('''class Program {
 public static borrowed Payload Work(STRONG<Payload> owner) { SharedView<Payload> view = owner.Borrow();
 borrowed Payload alias = view.Get(); return alias; } public static int Main() { return 0; } }''',
                                      'cannot be returned'),
        'parameter_view_get_finished': ('''class Program {
 public static int Work(STRONG<Payload> owner) { { SharedView<Payload> view = owner.Borrow();
 borrowed Payload alias = view.Get(); if (alias.Data != 42l) return 1; }
 owner = SharedPointers.MAKE<Payload>(); return owner.Get().Data == 42l ? 42 : 2; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'run:42'),
        'parameter_get_move': ('''class Program {
 public static int Work(STRONG<Payload> owner) { borrowed Payload alias = owner.Get();
 STRONG<Payload> other = move(owner); return (int)alias.Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'cannot move'),
        'parameter_get_overwrite': ('''class Program {
 public static int Work(STRONG<Payload> owner) { borrowed Payload alias = owner.Get();
 owner = SharedPointers.MAKE<Payload>(); return (int)alias.Data; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'cannot overwrite owner'),
        'parameter_get_return': ('''class Program {
 public static borrowed Payload Work(STRONG<Payload> owner) { borrowed Payload alias = owner.Get();
 return alias; } public static int Main() { return 0; } }''', 'cannot be returned'),
        'parameter_get_mutate': ('''class Program {
 public static int Work(STRONG<Payload> owner) { borrowed Payload alias = owner.Get();
 alias.Data = 9l; return 0; } public static int Main() { return 0; } }''', 'not assignable'),
        'parameter_get_finished': ('''class Program {
 public static int Work(STRONG<Payload> owner) { { borrowed Payload alias = owner.Get();
 if (alias.Data != 42l) return 1; } owner = SharedPointers.MAKE<Payload>();
 return owner.Get().Data == 42l ? 42 : 2; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'run:42'),
        'parameter_view_scope_finished': ('''class Program {
 public static int Work(STRONG<Payload> owner) {
 { SharedView<Payload> view = owner.Borrow(); if (view.Get().Data != 42l) return 1; }
 owner = SharedPointers.MAKE<Payload>(); return owner.Get().Data == 42l ? 42 : 2; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>()); } }''', 'run:42'),
        'other_parameter_unborrowed': ('''class Program {
 public static int Work(STRONG<Payload> owner, STRONG<Payload> other) {
 SharedView<Payload> view = owner.Borrow(); other = SharedPointers.MAKE<Payload>();
 return view.Get().Data == 42l && other.Get().Data == 42l ? 42 : 1; }
 public static int Main() { return Work(SharedPointers.MAKE<Payload>(), SharedPointers.MAKE<Payload>()); } }''',
                                       'run:42'),
        'async_overwrite_live_view': ('''class Program { public static async int Slow() { yield; return 7; }
 public static async int Main() { STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 SharedView<Payload> view = owner.Borrow(); int value = await Slow();
 owner = SharedPointers.MAKE<Payload>(); return (int)view.Get().Data + value; } }''', 'cannot overwrite owner'),
        'overwrite_finished_view': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>();
 { SharedView<Payload> view = owner.Borrow(); if (view.Get().Data != 42l) return 1; }
 owner = SharedPointers.MAKE<Payload>(); return owner.Get().Data == 42l ? 42 : 2; } }''', 'run:42'),
        'return_view': ('''class Program { public static SharedView<Payload> Leak() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); return owner.Borrow(); }
 public static int Main() { return 0; } }''', 'cannot be returned'),
        'temporary_owner_view': ('''class Program { public static int Main() {
 SharedView<Payload> view = SharedPointers.MAKE<Payload>().Borrow();
 return (int)view.Get().Data; } }''', 'temporary receiver'),
        'temporary_owner_get': ('''class Program { public static int Main() {
 borrowed Payload alias = SharedPointers.MAKE<Payload>().Get();
 return (int)alias.Data; } }''', 'borrowed local requires a direct live owner'),
        'temporary_owner_get_unqualified': ('''class Program { public static int Main() {
 Payload alias = SharedPointers.MAKE<Payload>().Get(); return (int)alias.Data; } }''',
                                            'borrowed value cannot be stored in unqualified local'),
        'return_view_class': ('''class Program { public static borrowed Payload Leak() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = owner.Borrow();
 return view.Get(); } public static int Main() { return 0; } }''', 'cannot be returned'),
        'write_view_class': ('''class Program { public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); SharedView<Payload> view = owner.Borrow();
 borrowed Payload alias = view.Get(); alias.Data = 9l; return 0; } }''', 'not assignable'),
        'capture_copy': ('''class Program { public delegate void Action(); public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 Action work = [copy alias] () => { alias.Data = 9l; }; work(); return 0; } }''',
                         'cannot be captured by a lambda'),
        'capture_ref': ('''class Program { public delegate void Action(); public static int Main() {
 STRONG<Payload> owner = SharedPointers.MAKE<Payload>(); borrowed Payload alias = owner.Get();
 Action work = [ref alias] () => { alias.Data = 9l; }; work(); return 0; } }''',
                        'cannot be captured by a lambda'),
        'nonthrowing_factory': ('''class Program { public static int Main() {
 try { STRONG<Bad> value = SharedPointers.MAKE<Bad>(); }
 catch (Exception error) { return 42; } return 1; } }''', 'emit:constructor throws'),
        'nonthrowing_value_factory': ('''class Program { public static int Main() {
 try { STRONG<BadValue> value = SharedPointers.MAKE<BadValue>(); }
 catch (Exception error) { return 42; } return 1; } }''', 'emit:constructor throws'),
        'empty_get': ('''class Program { public static int Main() {
 STRONG<Payload> owner = default(STRONG<Payload>); return (int)owner.Get().Data; } }''', 'trap:142'),
        'empty_view': ('''class Program { public static int Main() {
 STRONG<Payload> owner = default(STRONG<Payload>);
 SharedView<Payload> view = owner.Borrow(); return (int)view.Get().Data; } }''', 'trap:142'),
    }
    selected = os.environ.get('NSC_SHARED_POINTER_CASES')
    if selected:
        requested = set(selected.split(','))
        unknown = requested.difference(cases)
        if unknown:
            raise ValueError('unknown shared pointer cases: ' + ', '.join(sorted(unknown)))
        cases = {name: case for name, case in cases.items() if name in requested}
    with tempfile.TemporaryDirectory(prefix='nsc-public-shared-') as temporary:
        root = Path(temporary)
        for source in sources:
            shutil.copyfile(source, root / source.name)
        project = root / 'Probe.nsproj'
        project.write_text('<Project><PropertyGroup><Name>Probe</Name><Language>3</Language></PropertyGroup>'
                           '<ItemGroup><Compile Include="*.ns" /></ItemGroup></Project>', encoding='utf-8')
        families = (('Rc', 'WeakRc'), ('Arc', 'WeakArc'))
        selected_family = os.environ.get('NSC_SHARED_POINTER_FAMILY')
        if selected_family:
            families = tuple(pair for pair in families if pair[0] == selected_family)
            if not families:
                raise ValueError('unknown shared pointer family: ' + selected_family)
        for strong, weak in families:
            for name, (body, expected) in cases.items():
                if strong != 'Rc' and name in (
                        'static_strong', 'static_weak', 'static_nested_struct', 'static_nested_generic',
                        'static_nested_arc', 'static_nested_base', 'static_polymorphic_container',
                        'static_interface_container', 'static_array', 'static_local', 'namespace_state'):
                    continue
                if name == 'local_reentrant_weak_count' and strong != 'Rc':
                    continue
                if name == 'atomic_reentrant_weak_count' and strong != 'Arc':
                    continue
                extra = root / 'PartialFields.ns'
                if name == 'normal_factory_partial_files_owned_order':
                    extra.write_text('partial class Normal { public owned Part first; }\n'
                                     'partial class Base { public owned Part first; }\n', encoding='utf-8')
                elif extra.exists():
                    extra.unlink()
                text = (prefix + body).replace('MAKE_THROWING', 'Make' + strong + 'Throwing')
                text = text.replace('MAKE', 'Make' + strong).replace('STRONG', strong).replace('WEAK', weak)
                (root / 'Main.ns').write_text(text, encoding='utf-8')
                result = run([compiler, '--check-project-method-bodies', project], root)
                output = result.stdout + result.stderr
                if expected and (expected.startswith('trap:') or expected.startswith('run:')):
                    assert result.returncode == 0, (strong, name, output)
                    for level in os.environ.get('NSC_SHARED_POINTER_LEVELS', 'O0,O2').split(','):
                        exe = root / (strong + '-' + name + '-' + level + '.exe')
                        command = [compiler, '--emit-project-oop-exe', project, '-o', exe, '-' + level]
                        if os.environ.get('NSC_SHARED_POINTER_REPEAT_CACHE') == '1':
                            command += ['--artifact-cache=' + str(root / 'cache'), '--time']
                        result = run(command, root)
                        assert result.returncode == 0, (strong, name, level, result.stdout + result.stderr)
                        result = run([exe], root)
                        expected_exit = int(expected.split(':', 1)[1])
                        assert result.returncode == expected_exit, (strong, name, level, result.returncode)
                        print(strong, name, level, 'exit=' + str(result.returncode), flush=True)
                        if os.environ.get('NSC_SHARED_POINTER_REPEAT_CACHE') == '1':
                            result = run(command, root)
                            assert result.returncode == 0, (strong, name, level, result.stdout + result.stderr)
                            output = result.stdout + result.stderr
                            assert 'compilation-bundle-cache: hit' in output, (strong, name, level, output)
                            print(strong, name, level, 'compilation-bundle-cache: hit', flush=True)
                            result = run([exe], root)
                            assert result.returncode == expected_exit, (strong, name, level, result.returncode)
                            print(strong, name, level, 'repeated exit=' + str(result.returncode), flush=True)
                    continue
                if expected and expected.startswith('emit:'):
                    assert result.returncode == 0, (strong, name, output)
                    result = run([compiler, '--emit-project-oop-exe', project, '-o',
                                  root / (strong + '-reject.exe'), '-O0'], root)
                    output = result.stdout + result.stderr
                    assert result.returncode != 0 and expected[5:] in output, (strong, name, output)
                    print(strong, name, 'rejected during emission', flush=True)
                    continue
                if expected:
                    assert result.returncode != 0 and expected in output, (strong, name, output)
                    print(strong, name, 'rejected during semantic analysis', flush=True)
                    if name in ('temporary_owner_view', 'temporary_owner_get', 'temporary_owner_get_unqualified',
                                'overwrite_explicit_overlap_view', 'struct_pattern_ref_escape'):
                        print(output, flush=True)
                    continue
                assert result.returncode == 0, (strong, name, output)
                if os.name == 'nt':
                    for level in ('O0', 'O2'):
                        exe = root / (strong + level + '.exe')
                        result = run([compiler, '--emit-project-oop-exe', project, '-o', exe,
                                      '-' + level], root)
                        assert result.returncode == 0, (strong, level, result.stdout + result.stderr)
                        result = run([exe], root)
                        assert result.returncode == 42, (strong, level, result.returncode, result.stdout, result.stderr)
                        print(strong, level, 'exit=42', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
