import re
import sys
from pathlib import Path


def require(label, pattern, text, minimum=1):
    count = len(re.findall(pattern, text, re.MULTILINE))
    if count < minimum:
        raise SystemExit(f"{label}: expected {minimum}, found {count}")


clang_ir = Path(sys.argv[1]).read_text(encoding="utf-8")
nsc_ir = Path(sys.argv[2]).read_text(encoding="utf-8")
for label, ir in (("Clang", clang_ir), ("NSC", nsc_ir)):
    require(label + " aligned loads", r"load atomic volatile ptr, ptr .* acquire, align 8", ir, 2)
    require(label + " aligned stores", r"store atomic volatile ptr .* release, align 8", ir, 2)
    require(label + " packed load", r"load volatile ptr, ptr .*, align 1\b", ir)
    require(label + " packed store", r"store volatile ptr .*, ptr .*, align 1\b", ir)
    require(label + " global acquire", r"load atomic volatile ptr, ptr @volatile_callback acquire, align 8", ir)
    require(label + " global release", r"store atomic volatile ptr .*, ptr @volatile_callback release, align 8", ir)
    require(label + " inherited load", r"load volatile ptr, ptr @inherited_callback_holder, align 8", ir)
    require(label + " inherited store", r"store volatile ptr .*, ptr @inherited_callback_holder, align 8", ir)
print("Callback volatile IR: aligned acquire/release and packed volatile verified.")
