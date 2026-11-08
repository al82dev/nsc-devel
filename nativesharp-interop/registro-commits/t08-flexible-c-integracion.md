# T08-010: integración de miembros de arreglo flexible C

Se adaptó el parche de Claude a la rama actual. Un miembro final `T data[]`
de un struct C se proyecta como `T[*]` cuando su elemento y alineación son
representables; ocupa cero bytes en el offset calculado por Clang. La misma
proyección se usa al reconstruir la metadata. La aserción anterior que
exigía rechazar este miembro se sustituyó por una que verifica tipo, offset,
tamaño y alineación, sin eliminar la comprobación del arreglo fijo.

## Evidencia y límites

- Compilación incremental de NSC: código 0; la compilación de la prueba de
  sesión seguía en curso al registrar el cambio. El código nuevo generó una
  advertencia de inicializador de campos opcionales.
- `pruebas-tanda-08-flexible-c/validar.cmd`, salida aislada
  `A:/nsc-interop-build/tanda08-flexible-integracion-20261107`: ejecución
  directa, Shared y Metadata O0–O3 y controles de alineación pasaron. La
  comparación de `_Bool` es informativa, aunque observó `i8` en ambos.
- El validador también ejecutó sondas que no forman parte de su condición
  de éxito: `sizeof` del miembro y conversión implícita a puntero se
  rechazaron; los retornos por valor `ProbeValueInts` y `ProbeValuePacked`
  terminaron con `-1073741819` (violación de acceso). Esta discrepancia ABI
  permanece abierta y no debe presentarse como capacidad funcional.
- Los casos `const` y desalineados siguen rechazados. No se ha ejecutado la
  validación completa de la tanda ni se ha cerrado T08-010.
