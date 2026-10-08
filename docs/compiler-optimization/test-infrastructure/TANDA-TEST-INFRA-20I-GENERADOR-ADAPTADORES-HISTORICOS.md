# Tanda 20I — Generador de adaptadores históricos

## Objetivo

Preparar la incorporación trazable de owners C++ históricos que todavía no
tienen ruta en el ejecutable consolidado, sin mantener cientos de wrappers
manuales ni fusionar owners en una misma unidad de traducción.

## Diseño

`generate_transitional_unit_adapters.py` consume un registro TSV explícito con
cuatro campos por fila: ID histórico, componente, owner y símbolo de entrada.
El generador valida el inventario completo antes de escribir:

- IDs y componentes con formato estable;
- owner relativo contenido por el directorio fuente;
- owner existente con entrada exacta `int main()` o `int main(void)`;
- ausencia de IDs, owners y símbolos duplicados.

Por cada fila genera un `.cpp` independiente que renombra exclusivamente
`main`, un `.def` de registro y un inventario CMake. La generación es iterativa,
`O(n)` en tiempo y espacio de metadatos, y no usa recursión.

## Residuos

Antes de regenerar se eliminan únicamente archivos `*Adapter.cpp` del
directorio exacto administrado. Otros archivos permanecen intactos. El
directorio de salida pertenece al árbol de build, nunca al código fuente ni a
C:.

## Validación dirigida

- Dos owners producen dos unidades de traducción distintas.
- Se rechazan identidades, owners o símbolos duplicados.
- Se rechaza un `main(int, char**)`, que requiere un adaptador explícito.
- La limpieza elimina sólo adapters obsoletos.

## Estado

`IMPLEMENTADO; AÚN NO INTEGRADO EN CMAKE`. El registro real y la integración se
harán en el siguiente commit para mantener una hipótesis causal por commit.
