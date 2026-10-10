# Tanda 04.02 — Implementación de Host.Frontend

Fecha: 2026-10-09

## Alcance

Se materializaron los `504` casos `TF-FRONT-001..504` de los contratos
`CF-FRONT-001..063`. El cambio pertenece exclusivamente a la suite nueva:

- no modifica código de producto;
- no toca la batería histórica ni sus seis archivos protegidos;
- no usa `C:`, despliegue, baseline ni remotos;
- no compila ni ejecuta pruebas o gates durante esta etapa.

## Organización implementada

Seis módulos `Frontend*Module.cpp` y un soporte aislado conservan los `63`
contratos observables del frontend. Cada registro materializa la matriz canónica
de ocho casos: nominal, clases válidas, vacío, límites, estado inválido, fallo de
dependencia, determinismo y escala adversarial.

La implementación cubre:

- entrada, etapas, unidades, namespaces, nombres e identidades precompiladas;
- miembros, ownership, enums, herencia, interfaces y despacho virtual;
- genéricos, restricciones, conversiones, constantes y trabajo por callable;
- orquestación de proyecto y construcción de method bodies;
- scopes, flow, statements, expresiones, llamadas, ownership y closures;
- construcción, destrucción, layout, comptime, macros y metaprogramming;
- lowering HIR/MIR y sus dumpers deterministas.

La integración declarativa incorpora:

- `504` filas canónicas en `manifest/fragments/host-frontend.jsonl`;
- `441` casos de perfil `PR` y `63` de perfil `NIGHTLY`;
- `20` contratos unitarios, `42` de componente y `1` de integración;
- `run_host_frontend_gate.py`, que exige exactamente
  `TF-FRONT-001..504` en orden canónico tanto en el manifiesto como en los
  resultados del controlador.

El fragmento se mantiene separado del manifiesto compartido para evitar una
colisión de edición durante la materialización paralela. Su fusión pertenece al
integrador de la ola y no cambia el denominador de `504` casos.

## Estados visibles

El plan canónico clasifica los `504` casos como `DESIGNED`. No declara para
Host.Frontend casos de infraestructura especial ni defectos de producto
preclasificados. Esto no convierte la materialización en verificación: cualquier
defecto que aparezca durante la validación offline deberá conservar el oráculo
real y permanecer visible.

## Estado de validación

```text
Implementación: 504/504 MATERIALIZADA
Registros: 63/63 MATERIALIZADOS
Fragmento de manifiesto: 504/504 MATERIALIZADO
Build Debug/Release: NO EJECUTADO
Pruebas: NO EJECUTADAS
Gate controlador: NO EJECUTADO
Motivo: validación offline diferida hasta completar toda la Fase 2
```

Los `504` casos aumentan la columna de implementación. Ninguno aumenta la
columna de verificación en esta tanda.
