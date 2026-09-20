# NativeSharp 2.0 self-hosting — tanda 40: regiones EH Win64

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH38` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `19f1827d8` |
| Commit de cierre | este commit (`selfhost(tanda-40): publish Win64 EH regions`) |

## Diseño e implementación

`ExceptionRegionGraph` recorre el árbol completo y cuenta `try`, `catch`, `finally`, `throw` y
`using` en una salida estable (`--dump-project-eh`). Ésta es la identidad común para lowering,
runtime y NativeSharp Debugger; no depende del perfil ni de direcciones del proceso.

El backend ejecuta la ruta normal de `try/finally`: baja el cuerpo protegido y, si éste continúa,
el finalizador en el mismo estado de función. La fixture incrementa el resultado en ambos bloques.
LLVM emite metadata de unwind Win64 para las funciones resultantes; el gate inspecciona
`UnwindInformation`, además de enlazar y ejecutar el objeto por ambas rutas.

## Gate SH38

- grafo exacto: un `try`, un `finally`, sin regiones espurias;
- `.pdata/.xdata` observable mediante `llvm-readobj --unwind` en O0–O3;
- resultado 42 con LLD y NSLink en los cuatro perfiles.

El gate certifica la ruta normal y la estructura/unwind de funciones. El lanzamiento y matching
de excepciones administradas requieren los servicios reconstruidos del runtime de SH48; esta tanda
no confunde la presencia de unwind prolog/epilog con una prueba de `throw/catch` ejecutado.

- [x] identidad determinista de regiones EH.
- [x] ruta normal `try/finally`.
- [x] metadata Win64 y ejecución O0–O3 con ambos linkers.
- [ ] baseline acumulado final.
