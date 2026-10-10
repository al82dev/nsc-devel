# Tanda Validacion 137 - Ejecucion y fallos exactos en Builder 3

Fecha: 2026-10-10  
Estado: VERIFICADA SIN EJECUTAR PRODUCTO

## Defecto

La pestana Construccion permitia configurar y construir la infraestructura de
pruebas, pero obligaba a descubrir en otra pestana la accion de ejecucion. El
motor ya conservaba las variantes terminales para reintento, aunque Resultados
solo mostraba un reporte textual: no exponia una tabla con los casos exactos ni
permitia apartar temporalmente uno de ellos de una reejecucion dirigida.

## Correccion

- `Ejecutar pruebas` aparece junto a `Construir infraestructura` y reutiliza la
  seleccion granular existente.
- Resultados contiene `Fallos exactos`, una fila por identidad estable
  `caso|build|optimizacion`.
- Las filas seleccionadas pueden desactivarse o reactivarse solamente para el
  reintento dirigido.
- `Reejecutar fallos activos` ejecuta exclusivamente las variantes visibles y
  activas; no ejecuta su componente, contrato u ola completos.
- Una seleccion normal nunca excluye silenciosamente las filas desactivadas:
  la desactivacion solo pertenece al plan de reintento.
- Los fallos corregidos desaparecen de la tabla cuando su variante vuelve a
  producir un resultado no terminal.

## Evidencia

| Comprobacion | Resultado |
|---|---:|
| Compilacion sintactica Python | PASS |
| Pruebas de proyeccion y desactivacion exacta | 7/7 PASS |
| Smoke de GUI con botones y pestana exigidos | PASS |
| Compilacion o ejecucion de NSC/producto | NO EJECUTADA |

No se modificaron compilador, controladores, casos, manifiestos, CMake, bateria
historica ni los seis archivos protegidos.

