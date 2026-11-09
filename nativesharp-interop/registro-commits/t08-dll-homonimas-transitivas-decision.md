# T08-002-09m7c1d2c: dependencias transitivas de DLL homónimas (comportamiento actual y decisión)

Fecha: 2026-11-08.

## Reproducción

Sonda `claude/sondas/transitiva1/sonda.py` (O0):

- Dos carpetas, cada una con su `api.dll` (TLS exportada) y su `dep.dll`. Cada `api.dll` importa su
  propia `dep.dll`; `dep_value()` devuelve 101 en una carpeta y 102 en la otra.
- Un productor precompilado por carpeta declara `api` y `dep` como `CppLibraryReference`. Al publicar
  una biblioteca, NSC exige declarar la dependencia de ejecución hermana: «runtime dependency must be
  declared for publication: dep.dll».
- Un consumidor usa la TLS y `api_dep()` de ambos productores.

Resultado:

```
nsc: error: runtime packaging failed: runtime package contains different DLLs named 'dep.dll':
'…/1/dep.dll' and '…/2/dep.dll'
```

Las `api.dll` homónimas sí se resuelven con los alias TLS existentes (`__nsc_tls_*.dll`). En cambio, la
dependencia transitiva homónima pero distinta se rechaza al empaquetar, con un diagnóstico que nombra
los dos archivos. No se publica un paquete con una elección arbitraria.

## Por qué no se resuelve aquí

El cargador de Windows resuelve las importaciones por nombre base y reutiliza un módulo que ya tiene
ese nombre cargado, sin importar su carpeta. Los dos alias de `api.dll` importan `dep.dll`; cargarlos
desde subcarpetas distintas no evita que ambos usen el mismo módulo `dep`. Para conservar las dos
`dep.dll` hace falta una de estas opciones:

1. Reescribir la tabla de importación de las copias alias para que apunten a copias renombradas de cada
   `dep.dll`. Hoy los alias son copias idénticas en bytes; con este cambio dejarían de serlo, lo que
   altera binarios de terceros y su firma.
2. Usar ensamblados side-by-side con manifiestos privados por alias. Cambia el empaquetado y su forma
   visible.
3. Mantener el rechazo actual como contrato y documentarlo.

Las tres tienen efectos visibles distintos. Queda como decisión pendiente; el rechazo actual es seguro.
