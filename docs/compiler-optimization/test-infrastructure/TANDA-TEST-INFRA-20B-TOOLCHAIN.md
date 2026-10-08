# Tanda 20B — Integración de toolchain en NSC Builder 2

## SCOPE LOCK de la subtanda

El cambio autorizado amplía exclusivamente Builder 2. Las fuentes de NSC,
NSLink, NSASM, NSDBG, el builder anterior y toda instalación en `C:` quedan
protegidos. La implementación puede generar comandos y operaciones de deploy,
pero durante esta subtanda no los ejecutará contra `C:`.

## Inventario de productores

| Productor | Fuente | Configuraciones | Productos consumibles |
|---|---|---|---|
| NSC | `A:\nsc-devel` | Release, Debug | `bin\nsc.exe` |
| NSLink | `A:\nslink-devel` | Release, Debug | `bin\<config>\nslink.exe` |
| NSASM | `A:\nsasm-devel` | Release, Debug | `bin\<config>\nsasm.exe`, `nsdump.exe`, `nsdis.exe` |
| NSDBG | `A:\nsc-debugger` | Release, Debug | `bin\<config>\nsdbg.exe`, `nsdbg.pdb` cuando exista |

La auditoría se obtuvo de los builders y CMakeLists vigentes en cada
repositorio. Builder 2 no duplicará su lógica de producto: conservará las
variables `*_PRODUCT_OUTPUT_DIR`, los targets oficiales y la resolución de
CMake/Ninja mediante el entorno.

## Flujo implementable

1. Construir NSC y emitir un recibo con configuración, ruta y SHA-256.
2. Construir cada herramienta seleccionada de forma independiente y emitir su
   propio recibo.
3. Verificar todos los productos declarados antes de considerar aprobado al
   productor.
4. Copiar herramientas seleccionadas al `bin` del build de NSC en A: mediante
   una operación de staging explícita.
5. Emitir un manifiesto que relacione productor, producto original, destino y
   SHA-256.
6. Al pulsar Deploy, validar nuevamente recibos y hashes; opcionalmente
   autogenerar primero las herramientas seleccionadas.
7. Instalar NSC en el prefijo elegido y copiar únicamente los productos
   seleccionados al `bin` resultante.

## Invariantes

- `Build compiler`, `Build tools`, `Stage tools`, `Configure tests`, `Run tests`
  y `Deploy` son operaciones distintas.
- Ninguna prueba reconstruye NSC ni una herramienta silenciosamente.
- Ningún deploy toma archivos de `C:`; `C:` es únicamente destino explícito.
- Un recibo deja de ser válido si cambia la ruta, configuración, tamaño, fecha
  o SHA-256 de cualquiera de sus productos.
- NSASM es una unidad de tres ejecutables; no se admite un recibo parcial.
- La ausencia del PDB opcional de NSDBG no invalida `nsdbg.exe`, pero el
  manifiesto registra si fue incluido.
- No se usa recursión para limpiar, descubrir, verificar ni copiar árboles.

## Complejidad

La verificación y el staging recorren una lista fija de productos: tiempo
`O(n + bytes)` por el hash y copia de cada archivo, memoria `O(1)` además del
buffer de I/O. No se recorre el repositorio completo ni se copian directorios
por conveniencia.

## Validación autorizada

- Pruebas unitarias de generación de planes, invalidación de recibos y
  manifiestos dentro de `A:\nsc-devel`.
- Comprobación sintáctica de Python y smoke de GUI.
- No ejecutar builds de productos, baseline, O1/O2/O3 ni deploy en `C:`.

## Estado

`DISEÑO APROBADO POR AMPLIACIÓN EXPLÍCITA DEL USUARIO; IMPLEMENTACIÓN EN CURSO`.
