"""Exercise ownership and exception paths in the corrected book examples."""
import os
from pathlib import Path
import shutil
import subprocess
import sys


compiler = Path(sys.argv[1]).resolve()
work = Path(sys.argv[2]).resolve()
repo = Path(__file__).resolve().parents[2]
cases = {
    '08': '''using NativeSharp.Core; using NativeSharp.BookExamples.BinarySearchTree;
        int Main() throws ArgumentException { raii BinarySearchTree tree = new BinarySearchTree();
            tree.Insert(new Student(2, "two", 90)); tree.Insert(new Student(1, "one", 80));
            tree.Insert(new Student(3, "three", 70)); bool duplicate = false;
            try { tree.Insert(new Student(2, "duplicate", 0)); }
            catch (ArgumentException error) { duplicate = true; }
            if (!duplicate || tree.Count != 3 || !tree.Remove(2) || tree.Count != 2) return 1;
            return tree.Find(1) != null && tree.Find(3) != null ? 0 : 2; }''',
    '11': '''using NativeSharp.Core; using NativeSharp.BookExamples.Deque;
        int Main() throws InvalidOperationException { raii ArrayDeque<WorkItem> queue = new ArrayDeque<WorkItem>(2);
            raii WorkItem first = new WorkItem(1, "first", false);
            raii WorkItem second = new WorkItem(2, "second", true);
            queue.AddBack(first); queue.AddFront(second); WorkItem[] slot = new WorkItem[1];
            queue.RemoveFront(slot); int value = slot[0].Id; queue.RemoveFront(slot);
            value = value * 10 + slot[0].Id; delete[] slot; return value == 21 ? 0 : 1; }''',
    '12': '''using NativeSharp.BookExamples.AvlTree;
        int Main() { raii AvlTree tree = new AvlTree();
            tree.Insert(new AvlRecord(2, "two")); tree.Insert(new AvlRecord(1, "one"));
            tree.Insert(new AvlRecord(3, "three"));
            if (tree.Insert(new AvlRecord(2, "duplicate")) || tree.Count != 3) return 1;
            if (!tree.Remove(2) || tree.Count != 2 || !tree.IsBalanced()) return 2;
            return tree.Find(1) != null && tree.Find(3) != null ? 0 : 3; }''',
    '13': '''using NativeSharp.Core; using NativeSharp.BookExamples.BinaryHeap;
        int Main() throws InvalidOperationException { raii JobPriorityQueue queue = new JobPriorityQueue(2);
            queue.Enqueue(new Job(1, "first", 1, 0)); queue.Enqueue(new Job(2, "second", 2, 1));
            Job item = queue.Dequeue(); int id = item.Id; delete item;
            return id == 2 && queue.Count == 1 ? 0 : 1; }''',
    '15': '''using NativeSharp.BookExamples.HashTable;
        int Main() { raii SymbolTable table = new SymbolTable(3);
            if (!table.Add(new Symbol("x", "type", 1))) return 1;
            if (table.Add(new Symbol("x", "other", 2)) || table.Count != 1) return 2;
            if (!table.Remove("x") || table.Count != 0) return 3; return 0; }''',
    '25': '''using NativeSharp.Core; using NativeSharp.BookExamples.DrozdekAdvanced;
        int Main() { raii ExpressionParser valid = new ExpressionParser("2 + 3 * (7 - 1) - 4 / 2");
            try { if (valid.Evaluate() != 18) return 1; }
            catch (InvalidOperationException error) { return 2; }
            raii ExpressionParser invalid = new ExpressionParser("1 / 0");
            try { invalid.Evaluate(); } catch (InvalidOperationException error) { return 0; }
            return 3; }''',
    '26': '''using NativeSharp.Core; using NativeSharp.BookExamples.DeitelNative;
        int Main() { int errors = 0;
            try { raii BankAccount invalid = new BankAccount("", 1); }
            catch (InvalidOperationException error) { errors = errors + 1; }
            try { raii BankAccount account = new BankAccount("a", 1); account.Deposit(0); }
            catch (InvalidOperationException error) { errors = errors + 1; }
            int[] grades = new int[] { 50, 101 };
            try { raii GradeBook invalidGrades = new GradeBook(grades); }
            catch (InvalidOperationException error) { errors = errors + 1; }
            delete[] grades;
            try { raii TimeOfDay invalidTime = new TimeOfDay(24); }
            catch (InvalidOperationException error) { errors = errors + 1; }
            return errors == 4 ? 0 : 1; }''',
    '27': '''using NativeSharp.BookExamples.DrozdekCore;
        int Main() { raii IntAvlTree tree = new IntAvlTree();
            tree.Insert(30); tree.Insert(20); tree.Insert(10); tree.Insert(25); tree.Insert(40);
            if (tree.Insert(25) || tree.Count != 5 || !tree.IsValid()) return 1;
            if (!tree.Remove(20) || tree.Count != 4 || !tree.IsValid()) return 2;
            return tree.Contains(10) && tree.Contains(25) && tree.Contains(30) && tree.Contains(40) ? 0 : 3; }''',
}
env = os.environ.copy()
env['TEMP'] = str(work)
env['TMP'] = str(work)
env.pop('NS_LIB_ROOT', None)
for number, source in cases.items():
    original = next((repo / 'book_examples').glob(number + '_*'))
    case = work / number
    shutil.copytree(original, case, ignore=shutil.ignore_patterns('obj', 'bin'))
    (case / 'Program.ns').write_text(source, encoding='utf-8')
    project = next(case.glob('*.nsproj'))
    for level in ('O0', 'O1', 'O2', 'O3'):
        executable = case / (level + '.exe')
        command = [str(compiler), '--emit-project-oop-exe', str(project), '-' + level,
                   '-o', str(executable), '--no-artifact-cache', '--no-banner']
        result = subprocess.run(command, cwd=case, env=env, capture_output=True, text=True, timeout=120)
        if result.returncode:
            raise RuntimeError((number, level, result.stdout, result.stderr))
        result = subprocess.run([str(executable)], cwd=case, env=env, capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise RuntimeError((number, level, result.returncode, result.stdout, result.stderr))
        print(number, level, 'PASS', flush=True)
