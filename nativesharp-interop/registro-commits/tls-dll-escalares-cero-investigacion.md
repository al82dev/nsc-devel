# Inicialización cero de TLS de DLL: investigación abierta

El fixture separado `validar-nsc-escalares.cmd` añade un global TLS C sin
inicializador explícito y otros escalares en una DLL. Antes de comprobar NSC,
un consumidor C enlazado por LLD llama a `tls_zero_read` dentro de la DLL y
exige cero. Esta referencia ha fallado; el ejecutable NativeSharp también
detectó que `tls_zero_read()` no devuelve cero. Por ello no se atribuye aún
el fallo al acceso generado por NSC.

`llvm-readobj` mostró `tls_zero` en RVA `0x6020` y el directorio TLS desde
`0x6000` hasta `0x6025`; los bytes de plantilla en ese desplazamiento son cero.
El fixture enlaza la DLL con `/noentry` y CRT estático solo para construir la
referencia. Falta investigar carga e inicialización TLS de esa configuración
antes de cambiar el compilador. Las salidas se conservan en
`A:/nsc-interop-build/tanda08-tls-directo/scalars`. La prueba está incompleta
y no se incorpora todavía como evidencia de éxito ni se modifica NSLink.

## Contraste posterior

`ScalarInspect.c` leyó índice TLS cero, dirección de slots no nula y bytes del
hilo distintos de la plantilla, tanto para `tls_zero` como para el entero
inicializado a cuatro. En una variante separada se añadió una función exportada
que referencia `GetCurrentThreadId`, introduciendo una importación Win32 en la
DLL: el control C pasó y NSC pasó los seis escalares O0–O3. Al repetir la DLL
sin esa importación el control C volvió a fallar. La diferencia observada no
identifica aún la causa y no autoriza sustituir la variante fallida. Los dos
scripts se conservan y se ejecutarán en la validación agrupada.
