# Tanda 26: deploy gráfico independiente

## Objetivo

Proporcionar una interfaz PySide independiente para instalar NativeSharp desde cualquier build de CMake hacia un directorio elegido por el usuario, sin modificar NSCBuilder ni reemplazar el acceso rápido `deploy.bat`.

## Diagnóstico

`deploy.bat` expresa correctamente el deploy cotidiano fijo de `A:\nsc-devel\build-clang` a `C:\native-sharp`. Convertirlo en un componente configurable habría eliminado precisamente esa simplicidad. La selección de rutas pertenece a otra herramienta.

## Cambios

- Se agregó una ventana `NativeSharp Deploy` con selección separada de build y destino.
- La operación usa `cmake --install <build> --prefix <destino>` sin pasar por un shell.
- Se valida `cmake_install.cmake`, la disponibilidad de CMake y destinos peligrosos como la raíz de una unidad.
- La salida de CMake se transmite en vivo y la operación puede cancelarse.
- Tras instalar, se exige `<destino>\bin\nsc.exe` y se ejecuta `nsc --version` para mostrar la identidad instalada.
- Las últimas rutas se recuerdan mediante `QSettings`.
- `--build` y `--prefix` permiten abrir la interfaz con rutas preseleccionadas.

## Archivos

- Nuevo `deploy.py`: aplicación PySide autocontenida.
- Existente `deploy.bat`: se incorpora al repositorio exactamente con el contenido creado por el usuario; no se modifica.
- Nuevo `docs/2.0/development/namespace-functions/TANDA-26-DEPLOY-GRAFICO-INDEPENDIENTE.md`: memoria de esta tanda.

## Mejora obtenida

El flujo fijo continúa siendo de un clic mediante el `.bat`, mientras que instalaciones alternativas pueden elegir origen y destino con confirmación, consola visible y verificación final. Ninguno de los dos flujos cambia NSCBuilder ni ejecuta pruebas.

## Verificación

- Análisis sintáctico de `deploy.py` sin generar archivos auxiliares.
- Importación de la aplicación con Qt fuera de pantalla.
- Validación del build existente `A:\nsc-devel\build-clang` y rechazo de una raíz de unidad como destino.
- Comprobación de que `deploy.bat` conserva exactamente su contenido original.
- No se ejecuta deploy, baseline ni CTest.
