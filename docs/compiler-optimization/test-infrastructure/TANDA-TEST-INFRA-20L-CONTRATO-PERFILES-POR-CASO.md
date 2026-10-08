# Tanda 20L — Contrato de perfiles por caso

## Problema

El modelo de manifiestos rechazaba cualquier perfil distinto de O0, aunque el
inventario histórico contiene casos cuya identidad y expectativa pertenecen a
O2 u O3. Aceptarlos sin otra regla también era incorrecto: el runner sustituye
el perfil del comando por el perfil elegido para la corrida y habría ejecutado
un caso O2 como O0 sin advertencia.

## Solución

- El modelo acepta exclusivamente `O0`, `O1`, `O2` y `O3`.
- Un caso con tag `fixed-profile` sólo participa cuando el perfil seleccionado
  coincide con `optimization` en su manifiesto.
- El tag histórico `o0-only` aplica la misma restricción a O0.
- Los casos sin esos tags siguen admitiendo el override explícito del runner.
- Pedir por ID un caso fijo con otro perfil produce un error claro antes de
  iniciar NSC.
- `--list` continúa enumerando todo el catálogo sin ejecutar ni ocultar casos.

## Validación dirigida

- Se cargaron manifiestos sintéticos O0, O1, O2 y O3.
- Cada caso fijo aceptó únicamente su perfil declarado.
- `Ofast` fue rechazado como perfil inválido.
- No se ejecutó NSC, O1, O2 ni O3.

## Alcance

Este cambio habilita la representación fiel de pruebas históricas de perfiles
altos; no las ejecuta. No modifica el compilador, CMake, el builder anterior,
C: ni las expectativas históricas.
