# T08-006: proveniencia de almacenamiento bool C

Se prepara una marca explícita de almacenamiento bool C en HIR, MIR, campos
globales e identidades de caché. El byte existente de MIR conserva el bit de
atomicidad y usa otro bit para la marca; los valores anteriores siguen
decodificándose. La representación LLVM aún debe adaptarse y validarse.
