# T14-004, T14-005 y T14-011: condiciones del preprocesador, identidad de la sesión y dependencias

## Comprobación

La [auditoría inicial](../TANDA-14-AUDITORIA-INICIAL.md) mostró que la sesión aplica `Define` y
`Undefine` al encabezado y que un cambio en un encabezado incluido indirectamente invalida la caché. Este
cambio lo comprueba con la caché de artefactos activa y contra un control de Clang en cada paso; no cambia
código de producción.

Causa de que funcione, en el código:

- las condiciones `#if` las decide el preprocesador de Clang con las opciones de la sesión
  (`buildClangInvocationArguments`), que recibe `Define` y `Undefine` del proyecto (T14-004);
- la identidad de configuración de la sesión (`CppSessionConfigurationIdentity.cpp`) incluye cada
  definición, con su acción y en orden, así que otro `Define` o un `Undefine` es otra identidad (T14-005);
- la sesión entrega cada archivo que abrió el `SourceManager` de Clang con su tamaño y su SHA-256
  (`sourceFileContentsSnapshot`), así que un cambio en un encabezado incluido por otro incluido invalida el
  resultado guardado (T14-011).

## Evidencia

Fixture nuevo `pruebas-tanda-14-configuracion/validar.py`, con y sin `--dll`. El encabezado elige una
función inline y una macro con `#if` sobre `T14_MODE` e incluye `middle.h`, que incluye `deep.h`. Con
`--artifact-cache` y el mismo directorio de caché se compilan, en orden: `T14_MODE=1` (111), otra vez sin
cambios (111), `T14_MODE=2` (122), `Undefine T14_MODE` después del `Define` (100) y un cambio en `deep.h`
(200). Cada ejecutable devuelve lo mismo que un control C++ de Clang 23.1.0 compilado con las mismas
definiciones, O0–O3. Con `--dll`, la función no inline está en una DLL.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t14-configuracion-9i36cxow` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t14-configuracion-g0rem1a8` |

En una corrida simultánea con otros cinco fixtures, el control C++ de Clang (no NSC) devolvió 0 en el último
paso de la variante DLL; repetida sola, pasa.

## Límites

- Los módulos C++ no participan en la identidad: una sesión no recibe interfaces de módulo
  ([t14-especiales.md](t14-especiales.md)).
