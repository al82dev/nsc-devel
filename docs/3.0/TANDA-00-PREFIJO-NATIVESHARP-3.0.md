# Tanda 00: prefijo operativo de NativeSharp 3.0

## Objetivo

Separar el desarrollo del compilador 3.0 de la instalación congelada de NativeSharp 2.0.

## Cambios

- Los scripts de build, deploy, diagnóstico, NSCBuilder y empaquetado usan por omisión `C:\native-sharp-3.0`.
- CMake y los comandos de proyecto descubren NSC y NSLink bajo el prefijo 3.0.
- Los históricos, gates y contratos finales de 2.0 conservan sus rutas originales.

## Archivos

Se actualizaron 20 archivos operativos de la raíz, `tools`, CMake, deploy y driver. No se incorporaron los cambios locales preexistentes de `NativeSharp.Core.IO` ni archivos sin seguimiento.

## Mejora

El desarrollo 3.0 puede instalarse y probarse sin sobrescribir `C:\native-sharp` ni contaminar el master binario 2.0.

## Verificación

Se validaron sintaxis Python, configuración CMake y ausencia del prefijo 2.0 en los archivos operativos modificados. No se ejecutó baseline, CTest ni deploy.
