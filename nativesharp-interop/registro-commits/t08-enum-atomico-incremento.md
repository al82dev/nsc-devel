# Tanda 8: incremento y decremento de enum C atómico

El análisis acepta `++` y `--` sobre enums importados de C. Las dos rutas de
HIR producen el operando entero `1` para estos enums y usan el `CompoundStore`
existente, que conserva si la expresión es prefija o sufija. En el caso
atómico, el backend emplea la ruta de comparación e intercambio ya incorporada.

Los cuatro casos prefijos y sufijos pasaron junto con la comparación de IR en
O0–O3, como global y como TLS. Evidencia:
`A:\nsc-interop-build\tanda08-atomic-enum-global-28197-9310` y
`A:\nsc-interop-build\tanda08-atomic-enum-tls-28207-8787`.
También se comprobó que una sonda de incremento de enum C no atómico genera
IR; esa sonda no acredita su ejecución. Quedan pendientes anchos, operaciones
y concurrencia adicionales, además de la validación completa de la tanda.
