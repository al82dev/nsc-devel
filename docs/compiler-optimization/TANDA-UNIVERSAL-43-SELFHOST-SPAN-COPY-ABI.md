# Tanda universal 43: copia de Span con ABI del tipo cerrado

## Causa confirmada

Después de corregir el CFG de SH41, `nsc1` producía `nsc2-o0-nslink.exe`, pero su auto-prueba era
intermitente: alternaba entre 42, `0xC0000005` y `0xC0000374`. La reproducción mínima
`--dump-source-info` falló 20/20 veces fuera de `GenerationArena`, lo que descartó la arena y la
consola como causa primaria.

La captura con `nsdbg` ubicó la violación en RVA `+0x269D`. El objeto la mapea a
`Span<T>.CopyTo`, exactamente en la carga indexada de `start[index]`. El backend de promoción emite
la declaración genérica abierta con `T` borrado como puntero; por ello generaba cargas y avance de
8 bytes aun para los tipos cerrados `Span<byte>` y `Span<char>`. El resultado era lectura fuera de
rango y corrupción del heap.

## Cambio

El backend reconoce `Span<T>.CopyTo`, `Span<T>.CopyToOverlapping` y
`ReadOnlySpan<T>.CopyTo` en el sitio de llamada, donde sí está disponible el tipo cerrado:

1. obtiene el tamaño físico real de `T` desde el modelo semántico;
2. carga puntero y longitud de ambos spans;
3. calcula el mínimo de las longitudes;
4. omite completamente la operación si el límite no es positivo;
5. emite `memcpy` para copia normal y `memmove` para copia solapada;
6. conserva como resultado el número de elementos copiados.

La operación sigue siendo O(n), pero elimina el bucle genérico físicamente incorrecto y delega la
copia continua al runtime nativo. No se modificaron pruebas, expectativas ni el contrato público de
`Span`.

## Archivos

- Modificado: `selfhost/nsc/CodeGen/LlvmBackend.ns`.
- Aclarado: `docs/compiler-optimization/TANDA-UNIVERSAL-42-SOURCE-LINE-STARTS.md`.
- Nuevo: este documento.

## Validación dirigida

1. Ejecutar solamente SH41 en O0.
2. Comprobar que `nsc2-o0-nslink.exe --self-test-basic` sea estable en ejecuciones repetidas.
3. Comprobar que `--dump-source-info` deje de producir corrupción del heap.
4. Mantener abiertos SH41 y sus cierres derivados hasta la corrida integral O0 con NSLink.
