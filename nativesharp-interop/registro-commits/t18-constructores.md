# Tanda 18, cambio 1: constructores C++ que pueden lanzar en locales NativeSharp

## Defecto

En la [auditoría inicial](../TANDA-18-AUDITORIA-INICIAL.md), ninguna clase de ICU (`ParsePosition`, `Edits`,
`ErrorCode`) se puede usar como local NativeSharp: `C++ local lifetime currently requires a non-throwing
constructor and destructor`. Sus constructores no son `noexcept`, como casi todos los de una biblioteca C++
real; el pendiente venía de la tanda 11 (constructores y destructores que pueden lanzar en locales).

Causa en el código: `ProjectCompilation` rechaza el local si el constructor o el destructor no son
`cppLifetimeNonThrowing` (`cppNoexceptLocalLifetimeOnly_`). Para el constructor la barrera ya no hace falta:
`lowerLocalDeclarationStatement` construye el local en su lugar y activa su indicador de destrucción solo
cuando el constructor vuelve, y la llamada al constructor desenrolla como cualquier llamada C++ (tanda 15),
así que una excepción deja el local sin construir y su limpieza omite el destructor, como en C++.

## Diseño

- Un local C++ admite un constructor que puede lanzar.
- Su destructor sigue sin poder lanzar (`noexcept(false)` se rechaza con `C++ local lifetime currently
  requires a non-throwing destructor`): un destructor que lanza al salir del ámbito durante otro
  desenrollado termina el proceso en C++, y ese camino no se probó para locales NativeSharp.

## Cambio

- `ProjectCompilation.cpp`: la barrera solo mira el destructor.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-18-constructores/validar.py`, con y sin `--dll`: un callback NativeSharp
construye `a(1)` y `b(-1)`; el constructor de `b` lanza `7`, la excepción atraviesa el frame NativeSharp hasta
el `catch` de la biblioteca, `a` se destruye una vez y `b` nunca. Resultado 107000012, igual que el mismo
callback en C++ (Clang 23.1.0), O0–O3. Control negativo: un local con destructor `noexcept(false)` se rechaza
con su motivo.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática (`--crt-startup`) | pasa (O0–O3) y negativo | `A:/temp/nsc-t18-constructores-uhay1dio` |
| DLL | pasa (O0–O3) y negativo | `A:/temp/nsc-t18-constructores-jawemr70` |

Con este cambio, las clases de ICU se usan como locales ([ICU](t18-icu.md)).

## Límites

- Destructores que pueden lanzar en locales NativeSharp.
