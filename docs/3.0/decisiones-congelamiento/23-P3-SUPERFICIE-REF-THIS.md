# Congelamiento 3.0 P3 — una biblioteca precompilada con métodos `ref` se puede consumir

Fecha: 2026-10-26. Revisión de partida: `b0a03bd98`.

## Problema

La superficie pública que genera una biblioteca escribe la vida inferida de un retorno `ref` de instancia:
`public ref[this] int Get() { }`. Al leerla, el consumidor daba «ref return of 'Get' names undeclared lifetime
'this'» (sondas RLib2Use con `ref` y RLibUse con `ref readonly`). Ningún método `ref` público de una `.lib` se
podía usar.

`validateReferenceReturnLifetime` revisaba `'this'` en un miembro estático. Después exigía que cualquier otra vida
estuviera declarada, sin exceptuar `'this'` en un miembro de instancia.

## SCOPE LOCK

- **Cambia.** `BuildAndDeclaration.cpp`: `'this'` explícito en un miembro de instancia no necesita declaración.
- **Idéntico.**
  - `'this'` en un miembro estático sigue rechazado.
  - Las vidas con nombre siguen exigiendo su declaración.
  - La superficie generada no cambia.

## Validación

| Sonda | Resultado |
|---|---|
| RLib2Use (`ref int Get()` de una `.lib`) | 6 |
| RLibUse (`ref readonly` método e indexador de una `.lib`) | 66 |
| RLibBad (escritura por esos `ref readonly`) | rechazada, «left side of assignment is not assignable» |

## Validación conjunta de los docs 20 a 23

nsc Release y `nsc_component_tests` Debug y Release recompilados con los cuatro cambios. Validado:

- los 48 casos TL-LANG de referencias, préstamos y vistas;
- el corpus de `await` (51 programas) contra `d04`;
- las sondas P2 de `d13`.

Los resultados están en los mensajes de commit.
