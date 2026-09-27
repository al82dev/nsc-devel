# Tanda 15 — instalación independiente desde NSC Builder

## Objetivo

Permitir que un build existente de NSC se instale desde la interfaz sin volver a configurar ni
compilar el proyecto y sin ejecutar pruebas.

## Cambio

La pestaña del compilador incorpora el botón `Instalar build existente`. La operación toma las
rutas visibles en `Build de nsc` e `Instalar en` y ejecuta únicamente:

```text
cmake --install <build> --prefix <destino>
```

Antes de iniciar se comprueba que el build contenga `cmake_install.cmake`. La operación reutiliza
el worker, la cancelación, la consola, la barra de progreso, el historial y los informes de errores
existentes. Mientras está activa, los botones capaces de iniciar otra operación permanecen
deshabilitados.

## Archivos modificados

- `nscbuilder.py`: añade el control visual, el plan `install`, la validación del build y la etapa
  exclusiva de instalación.

## Archivo nuevo

- `docs/compiler-optimization/TANDA-15-nscbuilder-install-button.md`: registra el propósito, el
  alcance y la validación de esta tanda.

## Mejora

Una compilación ya terminada puede publicarse en `C:\native-sharp` —o en el prefijo configurado—
con un clic. Esto evita una reconstrucción accidental cuando solamente se necesita actualizar la
instalación local.

## Compatibilidad

El botón anterior `Construir e instalar nsc` conserva su comportamiento. La instalación utiliza
el mecanismo estándar de CMake y no cambia NSC, la ABI, NSMETA, los bindings, las librerías ni los
proyectos `.nsproj`.

## Verificación dirigida

- Comprobación de sintaxis de `nscbuilder.py`.
- Prueba aislada del plan `install` con la ejecución de procesos sustituida por un registro en
  memoria: se exige exactamente una etapa `Instalar` y ningún paso de configuración o build.
- No se ejecutó el baseline ni ninguna matriz de pruebas.
