# Tanda 27: paquete 7z versionado con raíz plana

## Objetivo

Generar una distribución de NativeSharp con nombre `native-sharp-<version>-<hash>-<timestamp>.7z`, usando `A:\7z\7z.exe` y colocando directamente en la raíz del archivo los elementos instalados en `C:\native-sharp`.

## Diagnóstico

Comprimir la carpeta `native-sharp` completa crea un nivel contenedor innecesario. Además, un nombre fijo no permite distinguir con certeza qué compilador contiene cada paquete. La identidad que publica `nsc --version` ya proporciona la versión y el hash exactos necesarios.

## Cambios

- Se agregó un empaquetador independiente con rutas predeterminadas para la instalación, 7-Zip y el destino `A:\`.
- El nombre se construye con versión, hash completo publicado por NSC y fecha/hora local.
- Los hijos inmediatos de la instalación se pasan individualmente a 7-Zip desde el directorio instalado; por ello no existe una carpeta contenedora `native-sharp` dentro del archivo.
- La salida se escribe primero como `.partial` y solo recibe el nombre final después de comprobarla.
- El paquete generado se vuelve a listar con 7-Zip para verificar rutas seguras y todos los elementos esperados en la raíz.
- `--dry-run` permite comprobar identidad, rutas, nombre y comando sin gastar CPU en compresión.
- Las rutas y el nivel de compresión se pueden cambiar mediante argumentos.

## Archivos

- Nuevo `package_native_sharp.py`: empaquetador y verificador de la distribución instalada.
- Nuevo `docs/2.0/development/namespace-functions/TANDA-27-PAQUETE-7Z-PLANO.md`: memoria de esta tanda.

## Mejora obtenida

Cada entrega queda identificada de manera inequívoca y lista para descomprimirse directamente en su destino, sin una carpeta adicional. Una interrupción o error de 7-Zip no deja un paquete final que parezca válido.

## Verificación

- Análisis sintáctico sin generar archivos auxiliares.
- Pruebas directas de lectura de formatos de versión antiguo y nuevo, formación del nombre y detección de raíz plana.
- Ejecución `--dry-run` contra `C:\native-sharp` y `A:\7z\7z.exe`.
- No se crea ningún `.7z`, no se despliega NativeSharp y no se ejecuta baseline ni CTest.
