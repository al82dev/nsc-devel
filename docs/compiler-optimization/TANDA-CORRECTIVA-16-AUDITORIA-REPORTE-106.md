# Tanda correctiva 16: auditoria del reporte de 106 fallos

**Fecha:** 2026-10-07  
**Repositorio:** `A:\nsc-devel`  
**Rama:** `development`

## SCOPE LOCK

- Objetivo autorizado: atribuir las doce causas unicas del reporte
  `20261006-212742-dd2722fb` y corregir exclusivamente productores del compilador que hayan
  cambiado durante la reingenieria de `method-bodies`/`construction`.
- Naturaleza: auditoria correctiva y comprobacion O0; no es una migracion de Core, runtime,
  pruebas, ejemplos ni proyectos.
- Comportamiento inmutable: las pruebas historicas, sus fuentes, proyectos y expectativas deben
  permanecer byte por byte intactos.
- Revision correcta: la bateria historica anterior al rango de rendimiento
  `45dea40ec..72f7ad7cb`.
- Archivos permitidos: productores de NSC bajo `include/` y `src/`, cobertura nueva separada y
  documentos bajo `docs/compiler-optimization/`.
- Archivos protegidos: todo archivo existente bajo `tests/`, `examples/` y
  `lib/NativeSharp.Core/`; otros repositorios e instalaciones.
- Acciones prohibidas: tocar o instalar en `C:`, ejecutar baseline o minibaseline, ejecutar
  O1/O2/O3, adaptar expectativas, copiar artefactos desde `C:`, usar agentes, introducir
  recursion o implementar features aplazados.
- Criterio de terminacion: cada causa se prueba contra el compilador actual; las causas del
  compilador quedan corregidas en su productor y cualquier causa fuera del alcance queda
  demostrada con archivo, linea y commit causal, sin encubrirla.

## Corte temporal del reporte

La corrida reportada comenzo a las 21:27 del 6 de octubre y utilizo el compilador con hash
`e25e6ebd6611cfa2`. Es anterior a estas correcciones ya incorporadas en `development`:

- `842bad7ed`: retiro la imposicion global de edicion, el marcador `ns3-value` de structs
  ordinarios y la exigencia global de `unsafe` para leer unions C;
- `0605c02c4`: limita el rechazo de escape a un contrato `Global` realmente probado;
- `f73f9d134`: mantiene `move(...)` como operacion optativa tambien para valores copiables.

Por eso las 106 filas del reporte no representan 106 defectos vigentes en el arbol actual.

## Evidencia dirigida con el compilador actual

Se reconstruyo unicamente `nsc` en `A:\nsc-devel\build-clang`. No se instalo ni se escribio nada
en `C:`. El ejecutable probado publica el hash `7011d119aa1f6db7`.

Cada proyecto se emitio y ejecuto individualmente en O0 mediante
`A:\nslink-devel\bin\release\nslink.exe`. Los temporales se eliminaron al terminar.

| Causa del reporte | Resultado actual |
|---|---|
| `c2_12_union` | compila; salida 42 |
| `c6_struct_math` | compila; salida 42 |
| `cc1_c27_algoritmos` | compila; salida 42 con Core O0 temporal A-local |
| `project_stage56_c2c4` | compila; salida normal 0 |
| `project_stage56_c6` | compila; salida normal 0 |
| `cc1-c43-igualdad-de-structs` | compila; salida 42 |
| `b4_this_by_value` | compila; salida 42 |
| `b5_ctor_owned_field_array` | compila; salida 42 |
| `b1_this_copy` | compila; salida 42 |
| `b3_ctor_aggregate` | compila; salida 42 |
| self-host / `Identifiers.ns` | marcador global retirado; cierre externo pendiente |
| `cc1_c23_views_and_lifetimes` | compila; salida 8, causa fuera del compilador demostrada abajo |

## Causa restante: cambio funcional de Core

El LLVM actual de `BoundsAreChecked` contiene tres `invoke`, tres handlers tipados y tres
incrementos del contador. `Optional<T>.Value()` conserva el `throw` y genera
`__nativesharp_throw` correctamente.

Las otras dos operaciones ya no lanzan porque sus fuentes fueron cambiadas por `9232bcfc0`
(`Sync bundled stdlib with Language 3 sources`):

- `lib/NativeSharp.Core/StringView.ns:143` devuelve `\0` fuera de rango;
- `lib/NativeSharp.Core/Span.ns:73` devuelve `default(T)` fuera de rango;
- `lib/NativeSharp.Core/Span.ns:81` convierte la escritura fuera de rango en no-op.

La revision padre de ese commit contenia `throw new ArgumentOutOfRangeException()` en los tres
caminos. El compilador actual ya soporta contratos `throws` en accessors mediante `5089a3db3`.

Esta causa no pertenece a la reingenieria de `method-bodies`/`construction` y no puede corregirse
honestamente desde NSC: inferir una excepcion ausente de la fuente inventaria semantica. Core esta
protegido por el `SCOPE LOCK`, por lo que no se modifica en esta tanda.

## Estado

- Productores del compilador observados en el reporte: corregidos y comprobados de forma dirigida.
- Pruebas, ejemplos, proyectos y Core: sin modificaciones.
- Baseline y minibaseline: no ejecutados.
- O1/O2/O3: no ejecutados.
- Residuo `error.log` de una invocacion invalida: eliminado.
- Archivo ajeno `_tmp_strip_saru_exceptions.py`: preservado sin versionar.
- Pendiente externo: una corrida O0 integral sobre los commits actuales.
- Bloqueo fuera de alcance: restaurar el contrato historico de rango de `StringView` y `Span`
  requiere una tarea autorizada sobre Core.
