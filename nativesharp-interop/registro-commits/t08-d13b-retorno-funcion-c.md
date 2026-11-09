# T08-D13b: `volatile T*` devuelto por una función C

Fecha: 2026-11-09. Decisión del usuario (punto 4): conservar en NSMeta la información necesaria para que el `volatile`
devuelto por una función C use el modo correcto. Debe mantenerse un único contrato vigente, sin versiones adicionales
ni compatibilidad histórica.

## Problema

Con `volatile int *vp_get(void);`, `var r = vp_get(); *r = 2;` emitía `volatile` simple también en modo Microsoft,
donde Clang emite acquire/release. El acceso directo `*vp_get() = 4` hacía lo mismo. Los símbolos de función C no
llevaban el modo de su referencia; las globales y los records sí lo llevaban.

## Cambio

**Símbolo de función** (`NamespaceFunctionSymbol::cppMicrosoftVolatileSemantics`).

- `collectCppSemanticFunctions` copia a cada función el modo de la sesión de su referencia
  (`session->microsoftVolatileSemantics()`), igual que con las globales.
- Al agrupar declaraciones de una misma función que vienen de referencias con modos distintos, el modo queda
  desconocido.

**Llamada** (`CallAnalyzer.cpp`). Cuando una función C importada devuelve un `volatile T*` con `T` escalar, el
resultado inicia el contrato de alias C de T08-D13 con:

- el modo de la referencia de la función;
- la alineación natural del dato apuntado, la misma que supone C.

Ese contrato se propaga a `var`, aritmética, índices y condicionales sin más cambios.

**NSMeta** (`CppCRecordMetadataCodec`). La instantánea `CDependencyFunctionSnapshot` guarda el modo de la referencia
del productor en un byte: 0 desconocido, 1 ISO, 2 Microsoft.

- Va después de los indicadores de la declaración, dentro del registro de función del contrato vigente. No se añadió
  ninguna versión nueva ni ninguna ruta de lectura para formatos anteriores.
- La captura toma el modo de la observación del productor.
- `restoreCppPrecompiledCFunctions` lo devuelve al símbolo restaurado. Si dos referencias del productor discrepan, el
  modo queda desconocido.

**Efecto sobre bibliotecas existentes.** Una biblioteca precompilada producida antes de este cambio con dependencias
de funciones C no se lee con el lector vigente, porque la decodificación canónica exige el byte nuevo. Debe
recompilarse, como cualquier biblioteca de un contrato anterior. Ninguna de las muestras históricas del repositorio
contiene dependencias de funciones C: todas las pruebas de metadata pasan salvo el fixture 1.46, que no existe.

## Prueba

Casos nuevos en `volatile-alias-c/validar-alias-modo.py`:

| Caso | Cuerpo |
|---|---|
| `Kreturn` | `var r = vp_get(); *r = 2; r[0] += 1; return *r;` |
| `Kreturndirect` | `*vp_get() = 4; return vp_get()[0];` |

**IR.** Se compara con Clang por función en Default, Microsoft e ISO, en O0–O3 en el camino directo.

**Precompilado.** En el consumidor sin encabezado salen 100 de 100 accesos atómicos en Default y Microsoft y 0 en
ISO. Eso solo ocurre si el modo viaja en la instantánea de función: sin él, el modo sería desconocido y los dos
modos darían `volatile` simple.

**Otros caminos.** Referencias mixtas (`volatile` simple), grafo de dos proyectos (cada uno con su modo), caché, y
ejecución en O0–O3 igual al control Clang (239).

**Rechazo conservado.** Un parámetro NativeSharp `int*` no recibe el alias, porque NativeSharp no escribe
`T volatile*` en firmas propias.

## Regresión

`claude\scripts\regresion-d13b.cmd`:

- **Conjunto de D13:** todo correcto salvo `diagnosticar-var-volatile.cmd`, que termina con 1 por diseño (compara
  las referencias mixtas con los dos modos de Clang).
- **Conjunto de D24** (catálogo NSMeta, identidad C, productores precompilados): resultado 0.
- **Conjunto de D05** (DLL TLS, precompilados con funciones C, rutas Unicode, plantillas): resultado 0.
- **ctest de metadata:** pasa todo salvo `nsc.stage48_2.library-metadata`, 442 de 444. Las dos que fallan son las
  del fixture histórico 1.46 que no existe ([investigación](t08-d24-fixture-historico-146.md)); fallaban igual
  antes de este cambio.

Los ejecutables de prueba que leen NSMeta se reconstruyeron antes de ejecutarlos (`claude\scripts\tests-exe.py`).
