# Congelamiento 3.0, elemento 141 — dueño inferido para `new` local

Fecha: 2026-10-27. Rama: `development`. Revisión de partida: `e38c08519`.

## SCOPE LOCK

- **Objetivo autorizado.** Evitar que una asignación nueva ligada directamente a un local léxico
  ordinario requiera repetir `raii`: `T value = new T(...)` y `var value = new T(...)` convierten ese
  local en dueño inferido.
- **Naturaleza.** Corrección del contrato C-01/DEC-02 antes del congelamiento.
- **Comportamiento que debe permanecer idéntico.** El dueño inferido usa exactamente el cleanup, `move`,
  control de escapes, captura y almacenamiento en frames que ya usa un local `raii` explícito.
- **Repositorios autorizados en este elemento.** `A:\nsc-devel`.
- **Archivos modificables.** `ProjectMethodBodyModel.h`, `StatementAnalyzer.cpp`,
  `Freeze30ContractTests.cpp`, este documento y `NSC-ESTADO-CONGELAMIENTO-3.0.md`.
- **Protegido.** Parámetros, campos, retornos, asignaciones posteriores a la declaración, arreglos,
  `scope`, locales estáticos, metadata, ABI, parser, sintaxis y bibliotecas.
- **Pruebas focales.** Local tipado, `var`, alias, captura, retorno con y sin `move`, async y generador.
- **Prohibido.** Batería completa, baseline, cambios masivos `raii`/`owned` en bibliotecas y ampliación
  de la inferencia a expresiones que no sean un `new` directo.
- **Criterio de terminación.** Las focales compilan y producen/rechazan exactamente lo documentado;
  `StringCast.ns` deja de requerir 158 anotaciones sin debilitar los demás destinos de C-01.

## Causa

C-01 trataba todo local sin `raii`/`owned` como referencia no dueña, incluso cuando su inicializador era
la asignación nueva que creaba el objeto. Al recompilar las bibliotecas, `StringCast.ns` produjo 158
diagnósticos idénticos sobre `StringBuilder sb = new StringBuilder();`. Cambiar esas declaraciones no
aporta información: el inicializador ya demuestra que el local recibe el único dueño posible.

## Contrato corregido

Un local infiere ownership léxico únicamente cuando se cumplen todas estas condiciones:

1. no es estático, referencia ni arreglo de longitud variable;
2. no declara `owned`, `borrowed`, `raii` ni `scope`;
3. no es un local sintético del compilador;
4. el inicializador es directamente `new T(...)`;
5. el tipo resultante es una referencia poseíble.

El local se representa con la misma bandera interna que un `raii` explícito. Por ello se destruye una vez
en todas las salidas, vive en el frame de una corrutina o generador, no se copia ni captura implícitamente
y sólo transfiere ownership mediante `move`.

No se infiere ownership en `x = new T()` después de declarar `x`, en campos, parámetros o retornos, ni para
un `new` descartado. Esos casos conservan los diagnósticos de C-01.

## Validación

- `nsc` compiló correctamente en `build-clang`.
- `nsc_freeze30_contract_tests` compiló correctamente; no se ejecutó la batería.
- O0 y O2, con `--no-artifact-cache`: los locales tipado y `var` devolvieron `41` y `51`;
  el retorno mediante `move` devolvió `61`; los casos async y generador devolvieron `21` y `71`.
- O0 y O2: alias, captura y retorno sin `move` se rechazaron con sus diagnósticos de ownership
  exactos.
- No se ejecutaron suites, baseline ni recompilación completa de bibliotecas. La comprobación del
  paquete `NativeSharp.Math` queda para el gate externo del usuario.
