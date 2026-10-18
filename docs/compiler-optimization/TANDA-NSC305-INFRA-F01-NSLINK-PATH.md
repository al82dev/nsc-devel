# NSC 3.0.5 — INFRA-F01: resolución de NSLink por nombre desde PATH

## Defecto demostrado

`LldLinker::findNsLink` trataba cualquier valor no vacío de `NSC_LINKER` exclusivamente como una
ruta. El valor `nslink.exe` era un nombre válido y `where.exe` lo resolvía, pero
`std::filesystem::is_regular_file("nslink.exe")` se evaluaba contra el directorio actual. La
función retornaba antes de ejecutar el buscador de `PATH` que ya usaba la ruta sin override.

## Corrección

La resolución distingue dos contratos:

- un valor con directorio conserva la ruta explícita y exige que el archivo exista;
- un nombre hoja `nslink` o `nslink.exe` recorre, en orden, los componentes del `PATH` nativo ya
  disponibles para el proceso. En Windows, `nslink` también prueba su forma `.exe`.

No se invoca shell, `where.exe`, MSYS ni un linker alternativo. No se modifica `PATH`, no se
agrega una ubicación absoluta de fallback y la búsqueda normal junto a `nsc.exe` permanece igual
cuando `NSC_LINKER` está ausente.

## Diagnósticos

- nombre ajeno: `NSC_LINKER must name nslink.exe: '<valor>'`;
- ruta explícita ausente: `NSC_LINKER executable does not exist: '<ruta>'`;
- nombre ausente del PATH: `NSC_LINKER executable '<nombre>' was not found in PATH`.

## Prueba focal

`WindowsLinkerTests` incorpora un fixture confinado a A: que comprueba:

1. `NSC_LINKER=nslink.exe` resuelve el archivo en el primer componente de `PATH`;
2. una ruta explícita sigue funcionando aunque `PATH` no contenga el ejecutable;
3. un nombre ausente produce el diagnóstico específico.

El fixture restaura las variables de entorno y se niega a escribir si el directorio de ejecución
no pertenece a A:. La validación se ejecuta además con `TEMP` y `TMP` en `A:\temp`.

## Evidencia

- build del target `nsc_windows_linker_tests`: PASS;
- CTest focal `nsc.windows-linker`: PASS 1/1 en 1.35 s;
- ejecución con `NSC_LINKER=nslink.exe` y `TEMP`/`TMP` confinados a `A:\temp`;
- sin cambios de sintaxis, API, ABI, NSMETA, formatos ni búsqueda normal sin override.
