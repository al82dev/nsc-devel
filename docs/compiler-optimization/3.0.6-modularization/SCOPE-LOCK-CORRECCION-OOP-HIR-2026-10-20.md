# SCOPE LOCK — corrección focal OOP-HIR (2026-10-20)

## Hecho causal

La corrida externa `20261020-125333-042f0fe2`, sobre `8bc5da56a`, terminó con
2,978 PASS, 18 TIMEOUT, 153 BLOCKED y 0 FAIL. Los diez casos `TF-FRONT` afectados
construyen HIR o MIR; los cinco productores self-host fueron detenidos en la etapa
`oop-hir`. La reproducción focal `TF-FRONT-473` tampoco terminó en 45 segundos.

## Contrato correcto

La construcción OOP-HIR debe terminar y conservar exactamente los contratos de
lenguaje, diagnósticos, HIR, MIR, LLVM IR, ABI, NSMETA, objetos, ejecutables, PDB,
dumps e identidad de caché existentes antes del refactor mecánico.

## Rango autorizado

- Diagnosticar y corregir únicamente la pérdida de progreso introducida al
  modularizar la máquina iterativa OOP-HIR.
- Archivos candidatos: `src/Frontend/HIR/ExpressionStatementMachine/`,
  `src/Frontend/HIR/HirExpressionMachine.cpp` y las declaraciones estrictamente
  correspondientes en `include/nativesharp/Frontend/ProjectOopHirModel.h`.
- No modificar casos, oráculos, manifiestos, CMake ni timeouts; la única
  excepción de infraestructura es la causa secundaria delimitada abajo.

## Causa secundaria expuesta por la focal real

Tras corregir OOP-HIR, `TL-LANG-291` alcanzó el enlace y reveló que la
configuración modular de NSC Builder 3 entrega al suite un NSLink hermano de la
modalidad (`build-clang-debug`) aunque NSC y NSLink fueron seleccionados juntos
en la configuración (`build-clang`). El monolito conservado ya usa la ruta
seleccionada; la divergencia fue introducida por `af16bbce0` y el ejecutable
referido no existe.

Para impedir que la siguiente corrida amplia falle después de superar HIR, se
autoriza exclusivamente restaurar esa paridad en
`nsc-builder/nsc_builder/build.py` y fijarla con una aserción en la prueba focal
existente de ensamblado de comandos. No se modifica la GUI, el runner, los
timeouts ni ningún caso del compilador.

## Validación permitida

- Construcción exclusiva del target `nsc_component_tests`.
- Prueba focal exacta `TF-FRONT-473`.
- Prueba focal exacta `TF-FRONT-497`.
- Prueba focal de ensamblado de la ruta NSLink seleccionada.
- Prueba focal exacta `TL-LANG-291` después de reconfigurar únicamente el árbol
  de su target con las mismas opciones ya registradas y la ruta corregida.
- Revisión estática del diff y del worktree.
- La suite completa queda reservada para Alberto desde NSC Builder 3.

## Cierre

La corrección queda lista cuando la prueba focal termina en PASS, no permanece
instrumentación diagnóstica y el diff no altera contratos ni algoritmos.

## Resultado

- `TF-FRONT-473`: PASS final, 78,556 microsegundos.
- `TF-FRONT-497`: PASS, 29,891 microsegundos.
- Prueba focal del comando NSLink seleccionado: PASS, 1/1.
- `TL-LANG-291`: PASS, 569,788 microsegundos.
- Productor `SelfHostBasic` O0/debug: compilación completa PASS en 1,376 ms;
  OOP-HIR terminó en 34 ms y OOP-MIR en 32 ms.
- No se ejecutó ninguna suite, batería ni baseline.
