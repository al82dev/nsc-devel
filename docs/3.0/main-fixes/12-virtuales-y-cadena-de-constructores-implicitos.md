# Llamadas virtuales sobre el objeto local y cadena de constructores implícitos (M1-03)

Rama `development-main-fixes`. Evidencia: `A:\nsc-main-fixes-evidencia\claude\m1-run` (sección `Square`) y
`logs\m1-03-*`.

## Llamadas virtuales sobre el local

`safeCall` ya resolvía una llamada virtual sobre el objeto local por su tipo exacto. Con el modelo de
despacho virtual, el destino efectivo es el método de la ranura del tipo exacto. El resumen general del
documento 10 se aplica a ese destino efectivo.

`m1-run` lo ejecuta: `Square square; square.Sides()` devuelve 4 (la sobrescritura), en O0 y O2.

## Causa del rechazo restante

Con `class Square : Shape` sin constructores explícitos, el objeto se rechazaba con «local class
construction or destruction is not proven safe». El modelo de construcción solo demostraba la inicialización
implícita por omisión de un tipo raíz; una clase derivada sin constructores nunca entraba a esa prueba.

## Contrato agregado

Una construcción `ResolvedImplicitDefault` sin constructor base explícito se demuestra segura cuando toda la
cadena de tipos hasta la raíz cumple estas condiciones:

- son clases fuente, sin campos no demostrados;
- no tienen constructor explícito, ni de entrada ni base directo;
- cada eslabón es `ResolvedImplicitBaseChain`, y la raíz es `NoBaseRequired`.

La cadena tiene un límite de 64 eslabones. La destrucción conserva su prueba propia
(`classLocalDestructionChainSafe_`).

## Pruebas

- `nsc.class-value-origins`: `LocalVirtualImplicitChain` llega a HIR como objeto del marco.
- `m1-run`: PASS O0 y O2.
- `nsc.ns3.t06.escape-summaries`, `nsc.stage50_7.borrow-escape-analysis` y
  `nsc.stage56.c7-definite-assignment` pasan.

## Límites

- Una llamada virtual sobre `this` dentro de un callee sigue siendo escape (documento 10).
- Un constructor base explícito con argumentos sigue fuera de la cadena implícita.
