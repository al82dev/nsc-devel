# Tanda: NSLink exclusivo para programas NativeSharp

## Alcance

- `nsc.exe` y las pruebas C++ del compilador pueden seguir construyéndose con
  `clang-cl` y `lld-link` como toolchain anfitrión.
- Todo ejecutable o DLL producido desde código NativeSharp se enlaza con
  NSLink. No existe descubrimiento ni fallback automático a `lld-link`.

## Cambios

- El enlazador de proyectos se descubre mediante `NSC_LINKER`, junto a
  `nsc.exe`, o como `nslink.exe` en `PATH`, en ese orden.
- `NSC_LINKER` sólo acepta un ejecutable llamado `nslink`/`nslink.exe`.
- `nsc build --linker` acepta únicamente `NSLink`.
- Se eliminaron las rutas absolutas a instalaciones antiguas del comando de
  proyecto.
- Los errores muestran el nombre real de NSLink o `llvm-lib`; ya no llaman
  `lld-link` a NSLink.
- Los contratos públicos anuncian únicamente NSLink para artefactos
  NativeSharp.

## Mejora

Un proyecto NativeSharp ya no puede cambiar silenciosamente de enlazador por
el contenido de `PATH`. La prueba ejercita exactamente el enlazador que se
entrega al usuario final y los diagnósticos identifican la herramienta real.
