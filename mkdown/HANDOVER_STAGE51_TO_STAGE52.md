# Handover — Stage 51 → Stage 52

## Estado al cierre

```text
nsc 0.1.0-dev51 (NativeSharp 0.1, stage 51)
Full CTest: 202/203 PASS (99%)
```

Única falla conocida, dejada deliberadamente sin forzar:

```text
nsc.stage50_10.ownership-examples-documentation::testPackagedGuidance
  -> busca contenido en un README.md empaquetado que no existe en este árbol.
     Gap de contenido histórico, no un bug de lógica del compilador. No se fabricó
     el archivo para forzar el PASS.
```

## Qué cambió respecto del paquete Stage 51 previamente entregado

1. **Fix real de compilador**: soporte de `enum` en el pipeline OOP (member model, method
   body model, HIR, MIR, LLVM lowering), reusando la infraestructura de Stage 37. Ver
   `STAGE51_CHECKPOINT.md` y el capítulo conceptual sección 4.
2. **Overloads reales con `StringComparison`** en `Equals`/`IndexOf`/`Contains`/
   `StartsWith`/`EndsWith`, habilitados por el fix anterior.
3. **Superficie de `String` completada**: 47→69 familias, 61→90 overloads. Familias nuevas:
   `Split`, `RSplit`, `SplitLines`, `Partition`, `RPartition`, `Count`, `RemovePrefix`,
   `RemoveSuffix`, `Compare`, `CompareOrdinal`, `CompareTo`, `Clone`, `ToLowerInvariant`,
   `ToUpperInvariant`, `Format`, `IsDecimal`, `IsNumeric`, `IsIdentifier`, `IsPrintable`,
   `IsTitle`, `ExpandTabs`, `ReplaceLineEndings`.
4. **`Driver.cpp` corregido**: ya no imprime "Stage 49" en `--help` ni en el mensaje de
   línea de comandos inválida.
5. **4 de 5 fallas de regresión heredadas, corregidas con causa raíz real**; la quinta
   documentada como gap honesto (ver arriba).
6. **Cierre mecánico real**: `MANIFEST_SHA256.txt` funcional (verificable con
   `verify_manifest.py`, ya incluido en el árbol), fresh-extraction + clean rebuild + full
   `ctest` desde esa extracción (ver `PACKAGE_VERIFICATION_STAGE51.md` para los números
   exactos de esa corrida), SOURCE-ONLY y FULL como ZIPs separados con SHA-256 publicados.

## Qué NO se tocó / queda explícitamente fuera de Stage 51

- `enum` anidado dentro de una `class` sigue sin parsear
  (`"error: expected an instance field or method declaration in class body"`). Es un límite
  de parser distinto del fix de esta ronda; no investigado a fondo.
- `Encode`/`Decode` real de `String` (más allá de ASCII) — requiere tabla de codificación.
- `ToString` de `float`/`double` — requiere algoritmo binario-a-decimal (Grisu/Ryu o
  equivalente), no trivial y deliberadamente fuera de alcance.
- Especificadores de formato dentro de `Format` (`{0:D2}`, etc.) — sólo posición de argumento
  soportada.
- Soporte Unicode real — `char` en NativeSharp es de 1 byte; todo el trabajo de Stage 51 es
  consistentemente ASCII.
- El `.exe` PE32+ resultante nunca se ejecutó en Windows real ni bajo Wine en este sandbox
  (sin acceso a ninguno de los dos entornos aquí). Se verificó en cambio: (a) el pipeline
  completo de compilación hasta objeto COFF/PE32+ real con `file` confirmando el formato,
  (b) un port línea-por-línea a Python de los algoritmos más delicados contra los mismos
  valores usados en los tests, para la lógica que no pudo ejecutarse nativamente.

## Recomendación para Stage 52

Dado el patrón de esta ronda (un gap señalado como "limitación permanente" resultó ser, en
realidad, un bug de compilador corregible reusando infraestructura existente), vale la pena
que Stage 52 empiece revisando específicamente los puntos listados arriba como "fuera de
alcance" con la misma pregunta que Alberto hizo esta vez: ¿es un límite arquitectónico real,
o un gap que no se investigó lo suficiente? En particular, `enum` anidado en clase parece
un candidato razonable a investigar primero, dado que ya se tocó el pipeline de enums en esta
ronda y el contexto está fresco.

## Cómo verificar este paquete de forma independiente

```bash
# 1. Verificar el manifest de integridad del árbol FULL
cd NativeSharp_Stage51_Current_Code
python3 verify_manifest.py
# esperado: "MANIFEST: N/N PASS"

# 2. Build limpio + suite completa
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
# esperado: 202/203 PASS, única falla nsc.stage50_10.ownership-examples-documentation

# 3. Identidad del compilador
./build/nsc --version
# esperado: nsc 0.1.0-dev51 (NativeSharp 0.1, stage 51)
```
