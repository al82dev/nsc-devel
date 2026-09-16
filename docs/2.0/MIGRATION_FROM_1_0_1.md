# Migración de NativeSharp 1.0.1 a 2.0

## Resumen

NativeSharp 2.0 conserva el formato NSMETA 1.20, el ABI nativo 1.0 y el ABI de runtime 1.0. Los
proyectos fuente deben recompilarse con el compilador 2.0 y revisar el ledger instalado en
`share/nativesharp/toolchain/NativeSharp2BreakingChanges.ledger`.

La ruptura que requiere acción está en los hosts nativos de bibliotecas estáticas. En 1.0.1 el
producto `.lib` podía incorporar el runtime Win32; en 2.0 el runtime es un producto independiente.
El enlace del host debe incluir, después de la biblioteca del usuario:

```text
NativeSharp.Runtime.Win32.lib
```

Un símbolo `__nativesharp_*` de runtime sin resolver durante el enlace indica que falta esa
biblioteca. No se debe copiar el runtime dentro de cada biblioteca ni depender de una inyección
implícita del linker.

## Procedimiento

1. Verifique la instalación 1.0.1 y conserve la lista de proyectos y productos distribuidos.
2. Instale 2.0 y ejecute `nsc --verify-installation <prefix>`.
3. Recompile todas las fuentes, bibliotecas precompiladas y ejecutables con 2.0.
4. En hosts C y C++ de bibliotecas estáticas, enlace `NativeSharp.Runtime.Win32.lib` después de la
   biblioteca NativeSharp consumida.
5. Ejecute las pruebas del proyecto en O0 y O2 y valide sus consumidores N-1/N.

---

# Migrating NativeSharp 1.0.1 to 2.0

NativeSharp 2.0 preserves NSMETA 1.20, native ABI 1.0, and runtime ABI 1.0. Rebuild every source
project and precompiled library with the 2.0 compiler and review the installed breaking-change
ledger. Native hosts of static libraries must explicitly link `NativeSharp.Runtime.Win32.lib`
after the user library. An unresolved `__nativesharp_*` runtime symbol diagnoses the missing link
input. Verify the installed package, rebuild all products, and run the project matrix in O0 and O2.
