NativeSharp SH44 — second-machine collection
================================================

This package is read-only input. Extract it to any writable drive on the second Windows x64 PC.
Do not place the output inside the extracted package.

Run from CMD.EXE (one line):

run_sh44_machine.cmd "%CD%" "%CD%\sh41" "X:\sh44-output" "C:\native-sharp" "C:\clang-llvm"

Replace X:\sh44-output, C:\native-sharp and C:\clang-llvm when that PC uses other locations.
Python and CMake must be available in PATH. The package carries the exact SH41 compilers and
Windows runtime library under test; it does not replace anything in C:\native-sharp. The command creates:

X:\sh44-output\machine-attestation.json

Copy only that JSON back to the development PC. Do not rename or edit it. The development machine
will rerun the matrix with the JSON as the sixth argument and will reject it if it came from the
same machine or if sources, tools, libraries or artifact bytes differ.
