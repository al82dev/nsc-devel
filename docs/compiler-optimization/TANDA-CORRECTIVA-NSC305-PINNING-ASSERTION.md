# Tanda correctiva NSC 3.0.5: expectativa MIR de pinning

## SCOPE LOCK

### Objetivo exacto autorizado

Cerrar el unico fallo restante de la prueba focal `NativeSharp3T06PinningTests` observado durante
la validacion del ABI de objetos. El binario NativeSharp compila, ejecuta y devuelve el valor
esperado; falla solamente la busqueda textual de atributos MIR.

### Naturaleza del cambio

Correccion de una expectativa de prueba obsoleta. No modifica compilador, lenguaje, ABI, metadata,
lowering ni runtime.

### Evidencia y causa

La prueba busca:

```text
reference-local=true pinned=true
```

El dumper vigente produce:

```text
reference-local=true reference-mutable=true pinned=true
```

La variable fuente es `pin ref int projected`; por contrato es simultaneamente referencia local,
mutable y pinned. La expectativa omite el atributo intermedio agregado al dumper y por ello no
encuentra una linea semanticamente correcta.

### Presupuesto de cambio

| Dimension | Autorizado |
|---|---|
| Repositorio | `A:\\nsc-devel` |
| Rama | `development` |
| Archivo funcional | `tests/unit/NativeSharp3T06PinningTests.cpp` |
| Simbolo | asercion `mirPasses` del caso positivo |
| Documento | este expediente |
| Prueba | `nsc_ns3_t06_pinning_tests` |

Todo lo demas permanece protegido.

### Cambio permitido

Actualizar la cadena esperada para incluir `reference-mutable=true`. No relajar la asercion, no
buscar atributos por separado y no eliminar ninguno de los tres contratos.

### Acciones prohibidas

- No cambiar el dumper MIR.
- No cambiar semantica de referencias, pinning, ownership o placement.
- No modificar el programa NativeSharp de la prueba.
- No tocar C:, X:, bindings, bibliotecas ni otros tests.
- No ejecutar baseline ni bateria completa.

### Criterio de terminacion

`nsc_ns3_t06_pinning_tests` debe pasar 8/8 usando linker, runtime, temporales y artefactos en A:.

## Resultado

```text
build focal: PASS
ejecucion focal: PASS 8/8
runtime: A:/nsc-devel/build-clang/generated/NativeSharp.Runtime.Win32.lib
linker: A:/nslink-devel/bin/release/nslink.exe
```

