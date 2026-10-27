# Formatos de inspección LLVM de un archivo fuente (NativeSharp 3.0)

Versión de este documento: 1 (2026-10-26). Origen: decisión DEC-05 del congelamiento 3.0.

## Alcance

El pipeline 3.x no tiene etapas propias de CFG, dominancia, SSA ni pases de MIR: su MIR es un árbol y LLVM hace
esas etapas. Las acciones de archivo de abajo inspeccionan el módulo LLVM que el pipeline 3.x genera para el
proyecto implícito del archivo (el archivo y una referencia a `NativeSharp.Core`). No corren el pipeline 1.x ni
adaptan su MIR a las librerías `src/CFG`, `src/Dominance`, `src/SSA` y `src/Passes`.

| Acción | Módulo observado | `-O` |
|---|---|---|
| `--dump-cfg`, `--verify-cfg` | LLVM sin optimizar | no lo cambia |
| `--dump-dominance`, `--verify-dominance` | LLVM sin optimizar | no lo cambia |
| `--dump-ssa`, `--verify-ssa` | LLVM sin optimizar, después de `mem2reg` sobre una copia | no lo cambia |
| `--dump-opt`, `--verify-opt` | LLVM optimizado (el mismo de `--dump-llvm-opt`) | elige el pipeline |
| `--dump-pass-pipeline` | pipeline LLVM del nivel | elige el pipeline |

Reglas comunes:

- El módulo se baja solo para la inspección y nunca se usa para generar código; la forma SSA se construye sobre una
  copia (`LLVMCloneModule`), así que la inspección no modifica nada.
- Salida determinista: funciones definidas en el orden del módulo (las declaraciones se omiten) y bloques en orden
  de disposición, numerados `bb0..bbN-1` por función; `bb0` es la entrada. Las listas de bloques van ordenadas.
- Una arista repetida (por ejemplo dos casos de `switch` al mismo destino) cuenta una vez.
- El módulo contiene todas las funciones que el pipeline materializa para el proyecto, incluidas las de
  `NativeSharp.Core` que el archivo alcanza.
- Un cambio de disposición de cualquiera de estos formatos sube su número de versión.

## `nsc-llvm-cfg/1` (`--dump-cfg`)

```
cfg.module nsc-llvm-cfg/1
function @<nombre> blocks=<n> edges=<e>
  rpo: [bb0, bb2, bb1]
  bb<i> "<nombre del bloque>" preds=[bb..] succs=[bb..] [entry] [unreachable]
```

- `rpo:` es el postorden inverso de los bloques alcanzables desde la entrada.
- El nombre del bloque aparece solo si LLVM tiene uno; `succs` sigue el orden del terminador.
- `[entry]` marca `bb0`; `[unreachable]` marca un bloque no alcanzable desde la entrada.

`--verify-cfg` imprime `CFG verification: ok`, `format: nsc-llvm-cfg/1` y las cuentas `functions`, `blocks`,
`edges` y `unreachable`. Falla si un bloque no termina en su terminador, si una arista sale de la función o si la
entrada tiene predecesores.

## `nsc-llvm-dominance/1` (`--dump-dominance`)

```
dominance.module nsc-llvm-dominance/1
function @<nombre> blocks=<n> reachable=<r>
  bb0 "<nombre>" idom=- depth=0 frontier=[]
  bb<i> "<nombre>" idom=bb<j> depth=<d> frontier=[bb..]
  bb<k> "<nombre>" [unreachable]
```

- Dominadores inmediatos por el algoritmo iterativo de Cooper, Harvey y Kennedy sobre el postorden inverso;
  fronteras por el método de los corredores. Sin recursión.

`--verify-dominance` imprime `Dominance verification: ok`, `format: nsc-llvm-dominance/1` y las cuentas
`functions`, `reachable` y `unreachable`. Comprueba el punto fijo del árbol: para cada bloque alcanzable que no es
la entrada, el dominador inmediato es alcanzable, va antes en el postorden inverso, tiene profundidad uno menor y es
el dominador común más cercano de los predecesores alcanzables.

## `nsc-llvm-ssa/1` (`--dump-ssa`)

```
; ssa.module nsc-llvm-ssa/1
; unoptimized LLVM of the implicit project after mem2reg on a copy
; function @<nombre> phis=<p> allocas=<a>
...
<texto IR de LLVM de la copia>
```

- Las líneas de encabezado son comentarios de IR; el resto es el IR textual de LLVM de la copia.
- `allocas` cuenta los que `mem2reg` no pudo promover (por ejemplo, una variable cuya dirección se toma). No es un
  error.

`--verify-ssa` imprime `SSA verification: ok`, `format: nsc-llvm-ssa/1` y las cuentas `functions`, `phis` y
`allocas`. Sobre la copia en SSA comprueba:

1. el verificador de LLVM (`LLVMVerifyModule`);
2. en cada bloque alcanzable, los phis forman un grupo al inicio;
3. cada phi tiene un bloque entrante por cada predecesor distinto, y ningún otro;
4. cada valor entrante definido por una instrucción domina el final de su bloque entrante;
5. cada operando de una instrucción que no es phi, definido por otra instrucción, la precede en el mismo bloque o
   está en un bloque que domina estrictamente al del uso.

No supone que desaparezcan todos los `alloca`.

## `nsc-llvm-opt/1` (`--dump-opt`, `--verify-opt`)

`--dump-opt` imprime `; optimized.ssa.module nsc-llvm-opt/1 <nivel>` y después exactamente la salida de
`--dump-llvm-opt` para el mismo nivel. `--verify-opt` imprime `Optimization verification: ok`,
`format: nsc-llvm-opt/1` y después la salida de `--verify-llvm-opt`.

## `nsc-llvm-pass-pipeline/1` (`--dump-pass-pipeline`)

Compila el archivo (los errores se reportan igual que en las otras acciones) e imprime:

```
pass.pipeline nsc-llvm-pass-pipeline/1
optimization-level: <O0|O1|O2|O3>
pass-pipeline: default<On>
target-aware: yes
verify-each: <yes|no>
```

Las cuatro líneas finales son las de `--print-llvm-pipeline` para el mismo nivel.
