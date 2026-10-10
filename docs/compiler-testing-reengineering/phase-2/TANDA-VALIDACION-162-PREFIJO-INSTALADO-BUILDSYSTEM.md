# Validacion 162: prefijo instalado para BuildSystem

## SCOPE LOCK

- Causa: `C12B-BLD-005` recibe un arbol de build como si fuera una instalacion.
- Casos afectados: `T12B-00025`, `T12B-00026`, `T12B-00029` y `T12B-00030`.
- Archivo de producto modificado: ninguno.
- Archivo de infraestructura modificado: `nscbuilder3.py`.
- Prohibido: copiar artefactos faltantes desde caches, relajar
  `VerifyInstalledNativeSharp.cmake` o adaptar NSC para aceptar un layout
  incompleto.

## Causa confirmada

`ExecutionEngine._run_buildsystem()` calculaba `installation_root` como el
abuelo del `nsc.exe` seleccionado. Para
`A:\nsc-devel\build-clang\bin\nsc.exe`, esa operacion produce
`A:\nsc-devel\build-clang`.

Ese directorio es un arbol de construccion. No es el prefijo instalado y no
contiene el contrato
`share/nativesharp/toolchain/NativeSharpInstallation.files` que exige
`VerifyInstalledNativeSharp.cmake`. El gate copiaba el arbol equivocado a su
sandbox y fallaba antes de verificar la instalacion.

## Correccion

Builder 3 entrega ahora `BuilderConfig.install_prefix`, que ya es un parametro
explicito, persistido y validado como ruta de `A:`. El gate conserva su copia
aislada y sigue comprobando el layout instalado completo; no se agrego ningun
fallback ni se fabricaron archivos.

## Contrato de validacion

La implementacion se commitea antes de probar. Despues del commit se ejecutan
puntualmente los cuatro casos afectados. El cierre de la Tanda 1 exige ademas
los cuatro productores self-host, sus `161` consumidores y finalmente el
conjunto correctivo de `439` variantes.

## Estado previo a ejecucion

- Implementacion: terminada.
- Validacion puntual: pendiente posterior al commit.
- Producto NSC modificado: no.
- Unidad `C:` modificada: no.
