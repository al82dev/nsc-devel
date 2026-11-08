# Conflicto de firma de función C precompilada

Una plantilla publicada llama `c_template_value()`; otro encabezado visible la
declara con un parámetro. Al usar ambas, NSC llegaba al verificador LLVM con
una llamada de firma inválida. `provideCppFunctionsWithPrecompiledCatalogs`
compara ahora las identidades canónicas cuando coinciden el símbolo externo C
y el USR, incluso entre proyectos, y diagnostica la incompatibilidad al
analizar. Las funciones coincidentes siguen disponibles por proyecto.

`plantilla-funcion/validar.cmd` pasó en O0: ejecutó los consumidores con y sin
encabezado compatible y comprobó el diagnóstico del caso incompatible. Se
compiló incrementalmente NSC en A:. La prueba cubre esta firma concreta y no
equivale a la batería completa de la tanda. El control demanda ambas
declaraciones; no acredita detección cuando el consumidor no usa la suya.
