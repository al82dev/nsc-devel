"""Check throwing field operators when a borrowed alias has two possible owners."""
import sys

import operator_increment_owned_field_throw_binary as common


common.RECEIVER = 'alias'
common.CONSUMER = '''using NativeSharp.Core;
class Program { public static int Main() {
 int caught = 0;
 { raii Holder first = new Holder(); raii Holder second = new Holder();
   borrowed Holder alias = first; alias = second;
   try { OPERATION; } catch (Exception error) { caught = caught + 1; }
   if (caught != 1 || first.Item.Value != 10 || second.Item.Value != 10 || Box.Count() != 0) return 1; }
 return Box.Count() == 2 ? 42 : 2; } }
'''


if __name__ == '__main__':
    sys.exit(common.main())
