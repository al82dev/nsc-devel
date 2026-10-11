# Tanda de validación 178 — `inlineAssemblyContract`

## Alcance

- Componente: `Host.LanguageService`.
- Contrato: `CF-LANGUAGE-SERVICE-018`.
- Casos afectados: `TF-LANGUAGE-SERVICE-139` y `TF-LANGUAGE-SERVICE-140`.
- Archivo: `tests/compiler-suite/host/modules/LanguageServiceDocuments081To160Module.cpp`.

## Causa

Las variantes de comentario y literal que contienen `asm {` usaban una función
compactada en una sola línea. Esa entrada no ejercitaba una edición observable
del formateador y el oráculo sólo comprobaba la presencia de cualquier edición,
sin verificar su resultado.

## Corrección

Ambas variantes ahora incluyen un cuerpo multilínea desalineado. Para las rutas
que no deben quedar protegidas se exige el texto formateado exacto; para las
rutas protegidas se mantiene la ausencia obligatoria de edición. No se modificó
el producto.

## Estado

Implementado. **PENDIENTE DE VALIDACIÓN FINAL** junto con el resto de la tanda;
no se compiló ni se ejecutó ninguna prueba durante este cambio.
