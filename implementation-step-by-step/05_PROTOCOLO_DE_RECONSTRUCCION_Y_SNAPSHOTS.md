# 05 — Protocolo de reconstrucción y snapshots

## 1. Objetivo de desastre

El repositorio literario debe sobrevivir al repositorio de código. Para conseguirlo, el texto por sí
solo no basta: cada checkpoint necesita materiales mecánicos que permitan reconstruirlo y verificar
que la reconstrucción no depende de memoria tácita.

## 2. Anatomía de un checkpoint

Cada checkpoint futuro tendrá esta forma conceptual:

```text
checkpoints/C042/
├── README.md
├── TREE.txt
├── MANIFEST_SHA256.txt
├── delta/
│   ├── FROM_C041.patch
│   └── APPLY.md
├── source/
│   └── ... snapshot o receta completa ...
├── fixtures/
│   ├── positive/
│   └── negative/
├── expected/
│   ├── diagnostics/
│   ├── dumps/
│   └── execution/
├── verification/
│   ├── COMMANDS.md
│   ├── RESULTS.md
│   └── environment.txt
└── provenance/
    └── SOURCES.md
```

No todos los checkpoints duplicarán todo el source si el volumen lo vuelve impráctico. En ese caso
habrá un baseline completo periódico más patches encadenados, y se probará reconstrucción desde el
último baseline.

## 3. Checkpoint cero

`C000` será una carpeta realmente mínima, no una copia mutilada del proyecto final:

```text
CMakeLists.txt
src/main.cpp
tests/ (si se introduce en C001)
README de construcción
```

El primer capítulo debe demostrar:

```text
directorio vacío
  → crear archivos
  → configurar CMake
  → compilar
  → ejecutar
  → exit code 0
```

## 4. Regla de deltas

Cada delta deberá ser:

- aplicable al checkpoint anterior sin editar a mano;
- lo bastante pequeño para revisar;
- acompañado por una explicación manual equivalente;
- verificado en una copia limpia;
- libre de paths absolutos de la máquina autora;
- capaz de fallar con mensaje claro si se aplica a la base equivocada.

## 5. Baselines completos

Se crearán baselines completos al menos en:

- frontend Stage 10;
- procedural Stage 20;
- OOP inicial Stage 30;
- RTTI/members Stage 39;
- exceptions Stage 45;
- ownership Stage 50;
- madurez Stage 57;
- producto 0.8;
- producto 0.9;
- E1–E4 certificado;
- release candidate;
- producto 1.0.0.

Podrán añadirse baselines intermedios cuando un formato de archivo o una toolchain cambie de forma
incompatible.

## 6. Manifest

Cada baseline completo tendrá SHA-256 de todos sus archivos relevantes. El manifest excluirá sólo
outputs reproducibles explícitamente documentados, nunca piezas necesarias para reconstruir.

La verificación tendrá dos estados:

```text
manifest del árbol fuente
manifest después de empaquetar y extraer
```

Ambos deberán coincidir. Un ZIP creado pero no extraído no cuenta como entrega certificada.

## 7. Identidad del ejecutable

Cada prueba registrará:

- path absoluto del `nsc` ejecutado en el log histórico;
- `nsc --version`;
- SHA-256 del binario;
- configuración (`Debug/Release`, O0/O2, linker);
- fuente desde la que se construyó;
- hora y entorno.

Esto evita repetir el defecto histórico de medir otro compilador por accidente.

## 8. Matriz de verificación

| Gate | Frecuencia | Propósito |
|---|---|---|
| build incremental | cada paso | feedback rápido |
| test focal rojo→verde | cada paso | prueba del delta |
| suite del subsistema | cada capítulo | regresión local |
| full CTest | cierre de capítulo de riesgo | regresión global |
| clean configure/build | cada checkpoint | ausencia de dependencia incremental |
| segunda toolchain | cierres de stage/baseline | portabilidad C++ y warnings |
| sanitizers | memoria/ownership/IR builders | errores de host compiler |
| LLVM verifier | cambios LLVM | validez estructural |
| ejecución O0/O2 | cambios semánticos | equivalencia observable |
| COFF/PE inspection | backend/linker/runtime | forma binaria |
| host independiente | ABI/interop | contrato productor-consumidor |
| ZIP extract/rebuild | baseline completo | integridad de distribución |
| installed gate | releases | identidad construida/instalada |

## 9. Positivos, negativos y corruptions

Cada feature tendrá:

- programa mínimo válido;
- programa integrado válido;
- programa mínimo inválido;
- borde numérico/layout/lifetime;
- artefacto corrupto si existe formato serializado;
- regresión de cada bug descubierto.

En NSMETA, COFF y PE se probará corrupción. En semántica se probará rechazo. En ownership se probará
exactamente un cleanup, no sólo ausencia de crash.

## 10. Evidencia independiente

Siempre que sea posible, la autoridad que verifica será distinta de la que genera:

- parser dump no es verifier del parser;
- SSA builder no verifica su propia dominance assumption;
- layout NativeSharp se compara con compilador C/C++ independiente;
- PE writer/linker se inspecciona con parser independiente;
- metadata writer se prueba con reader/verifier y golden bytes;
- built binary se compara byte/hash con installed binary.

## 11. Entornos sin Windows

Un caso que requiere ejecución Win64 no se marcará PASS si sólo se verificó estructura. Sus estados
serán:

```text
PASS_EXECUTION
PASS_STRUCTURAL_ONLY
SKIP_NO_WINDOWS
FAIL
NOT_RUN
```

La obra explicará qué conclusiones permite cada estado.

## 12. Reconstrucciones inferidas

Si un snapshot temprano no se recupera, se podrá crear un checkpoint reconstruido, pero deberá:

- llamarse `RECONSTRUCTED`, nunca “original”;
- listar las fuentes usadas;
- señalar decisiones inferidas;
- comparar su API/comportamiento con snapshots posteriores;
- conservar preguntas abiertas;
- sustituirse o corregirse si aparece el artefacto original.

## 13. Paquete final de la obra

El paquete final incluirá:

- todos los Markdown;
- catálogo de fuentes;
- checkpoints y deltas;
- fixtures y outputs esperados;
- manifests;
- scripts de verificación legibles;
- índices por stage, versión, símbolo, feature y bug;
- acta de reconstrucción completa desde `C000` a `C199`.

La prueba final de la documentación será entregar la carpeta a un entorno limpio y reconstruir sin
consultar el repositorio original.

