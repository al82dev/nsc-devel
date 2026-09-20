# NativeSharp 2.0 self-hosting — tanda 42: dependencias bootstrap desde fuente

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH40` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `60bf20ecf` |
| Commit de cierre | este commit (`selfhost(tanda-42): close source bootstrap dependencies`) |

## Implementación

`--verify-bootstrap-dependencies` construye `SourceGraph` y `ModuleSet` desde el proyecto real del
compilador. Exige cinco proyectos, cuatro dependencias externas y al menos noventa unidades; publica
los conteos de unidades y resoluciones semánticas sin aceptar metadata binaria como sustituto.

El gate conserva y compara tres artefactos por perfil: resumen, grafo fuente completo y conjunto de
módulos. La salida actual certificada es 5 proyectos, 96 unidades, 26 unidades raíz y 3,269
resoluciones externas. El verificador exige los owners Core, Collections.Generic, IO, LLVM.C y
Compiler y falla si el grafo contiene `.obj` o `.exe`.

Esto prueba que todas las dependencias que necesita el bootstrap son analizables y resolubles desde
fuente por `nsc-next`. La producción de `nsc2` queda, correctamente, como condición separada de SH41;
SH40 no presenta un parseo verde como si ya fuera una autocompilación.

- [x] cinco proyectos/96 unidades desde fuente.
- [x] semántica multimódulo sin entradas binarias ocultas.
- [x] identidad O0–O3.
- [ ] primera generación autónoma (SH41).
- [ ] baseline acumulado final.
