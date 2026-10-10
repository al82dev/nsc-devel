# Validación 09 — Catálogo canónico descubierto dinámicamente

## Propósito

Impedir que Builder 3 rechace una revisión válida sólo porque cambió el número
de componentes o casos de la suite del compilador.

## SCOPE LOCK

- Repositorio: `A:\nsc-devel`.
- Componente: catálogo canónico de NSC Builder 3.
- Archivo funcional: `nscbuilder3.py`.
- Símbolos: `CanonicalCatalog.load` y
  `CanonicalCatalog._validate_sources`.
- Protegidos: casos, contratos, oráculos, disposiciones, manifiestos, producto,
  suite histórica, seis archivos preexistentes, `C:`, despliegues y remotos.
- Validación funcional: pendiente de la siguiente corrida externa.

## Defecto

El catálogo leía las fuentes canónicas, pero después exigía exactamente `44`
componentes y `9,587` casos. Esos números describen la revisión actual; no son
un contrato estable. Una adición o retirada legítima obligaba a modificar el
orquestador aunque todas las fuentes fueran coherentes.

## Cadena causal

1. Las olas definen qué componentes pertenecen a la suite de NSC.
2. Diseño y asignación aportan el denominador actual para esos componentes.
3. El manifiesto aporta el subconjunto ejecutable.
4. Los dos totales escritos en código se evaluaban después de comprobar las
   relaciones y podían rechazar un inventario íntegro.

## Corrección

Los totales rígidos se sustituyen por once invariantes relacionales:

1. existe al menos un componente de compilador;
2. los componentes de las olas son únicos;
3. existe al menos un caso del compilador;
4. los IDs de diseño son únicos;
5. los IDs de asignación son únicos;
6. los IDs ejecutables son únicos;
7. ningún caso del diseño carece de asignación;
8. la asignación no contiene casos ajenos al diseño;
9. cada componente de las olas tiene casos;
10. cada asignación conserva el componente del diseño;
11. el manifiesto no contiene casos ajenos al diseño.

El filtro de componentes continúa siendo propiedad de
`implementation-waves.csv`; las 606 filas de bibliotecas permanecen fuera de
la suite de NSC conforme al `SCOPE LOCK`.

## Evidencia estática

- Componentes descubiertos en la revisión actual: `44`, todos únicos.
- Casos totales en diseño y asignación: `10,193/10,193`.
- Casos ajenos a la suite de NSC: `606/606`, filtrados por las olas.
- Casos canónicos actuales de NSC: `9,587`.
- Totales rígidos restantes en `CanonicalCatalog`: `0`.
- Casos, manifiestos y oráculos modificados: `0`.
- Recursión añadida: `0`.
- Complejidad: `O(n)` tiempo y `O(n)` espacio para índices de integridad.

## Estado

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE EJECUCIÓN PENDIENTE DEL USUARIO`.
