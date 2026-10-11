# Tanda de validación 179 — `formattingEditContract`

## Alcance

- Componente: `Host.LanguageService`.
- Contrato: `CF-LANGUAGE-SERVICE-019`.
- Casos afectados: `TF-LANGUAGE-SERVICE-145`, `146`, `149`, `150`, `151` y `152`.
- Archivo: `tests/compiler-suite/host/modules/LanguageServiceDocuments081To160Module.cpp`.

## Causa

Se reutilizaba la misma fuente compacta para variantes con contratos distintos.
El oráculo sólo exigía una edición no vacía y un rango fijo, por lo que no
representaba documentos vacíos, UTF-8, CRLF, determinismo ni escala.

## Corrección

Cada variante ahora construye su entrada y resultado exactos:

- documento sin cambios y documento vacío: ninguna edición;
- comentario UTF-8: eliminación precisa de espacios finales;
- saltos LF y CRLF: contenido y rango completos;
- repetición: misma edición en llamadas sucesivas;
- escala: 256 bloques formateados y rango completo del documento.

El oráculo compara posición inicial, posición final y texto íntegro. No se
modificó el producto.

## Estado

Implementado. **PENDIENTE DE VALIDACIÓN FINAL** junto con el resto de la tanda;
no se compiló ni se ejecutó ninguna prueba durante este cambio.
