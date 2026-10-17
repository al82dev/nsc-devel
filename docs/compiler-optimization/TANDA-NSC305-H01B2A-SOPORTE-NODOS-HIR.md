# NSC 3.0.5 — H01B2A: soporte común de creación de nodos HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Aislar `makeNode` en un header interno para que `buildImpl` pueda moverse sin duplicar el helper compartido por el lowering. |
| Naturaleza | Movimiento puro; misma firma, cuerpo y 357 sitios consumidores sin editar. |
| Repositorio/rama | `A:\nsc-devel`, `development`. |
| Archivos autorizados | `ProjectOopHirModel.cpp`, `HIR/HirLoweringSupport.h`, este expediente y tracking. |
| Símbolo | Exclusivamente `makeNode`. |
| Protegido | Todos los consumidores, `buildImpl`, statements, expressions, HIR/MIR, sintaxis, semántica, ABI, NSMETA y diagnósticos. |
| Terminación | Una sola definición fuente, cero cambios en call sites, `nsc` construido y pruebas HIR focales verdes. |

## Causa y cambio

`buildImpl` y las rutas de lowering comparten `makeNode`. Mantenerlo en el namespace anónimo del
monolito impediría compilar el coordinador como translation unit independiente; copiarlo produciría
dos propietarios editoriales. El helper se mueve, sin reescribirlo, a un header privado de `src`.

No se publica API, no se cambia el header público y no se modifica ningún consumidor.

## Resultado

- `makeNode`: una definición y 357 call sites sin editar.
- `git diff --check`: limpio.
- `nsc` construido; contrato de runtime estático aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.

H01B2 permanece abierto por H01B2B: movimiento de `buildImpl` y los helpers privados exclusivos
del coordinador.
