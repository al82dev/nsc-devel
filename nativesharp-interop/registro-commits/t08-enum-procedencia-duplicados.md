# Procedencia de enums precompilados duplicados

La comparación de duplicados precompilados usa ahora el envoltorio de enum
con procedencia cuando existe, para no descartar ese dato al formar la
representación canónica. No cambia el formato anterior cuando no hay datos de
origen. Falta la validación agrupada.
