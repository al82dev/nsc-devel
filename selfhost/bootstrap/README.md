# Infraestructura de bootstrap

SH00 prepara el entorno sin portar todavía código del compilador.

## Archivos normativos

- `bootstrap.lock`: identidades y hashes de la semilla y herramientas externas.
- `generations.json`: identidad y reglas de aislamiento de las cuatro generaciones.
- `dependencies.md`: inventario de dependencias permitidas.
- `bootstrap.config.example.json`: ejemplo de configuración independiente de la máquina.
- `verify_environment.py`: verificador sin dependencias Python externas.
- `bootstrap-profile.json`: contrato ejecutable del Bootstrap Profile 2.0.
- `verify_profile.py`: filtro obligatorio de fuentes y proyectos previo a cada generación.
- `clean_selfhost.py`: limpieza dirigida, seca por omisión y protegida por marcador.
- `recovery.md`: recuperación de la última semilla válida.
- `schemas/`: contratos JSON versionados para lock, reportes y gates.

Las rutas físicas no se guardan en el lock. Se resuelven mediante variables de configuración y la
evidencia registra las rutas absolutas efectivamente utilizadas.

SH01 congela el perfil. El comando mínimo para proteger las fuentes es:

```text
python -B selfhost/bootstrap/verify_profile.py --source-root selfhost/nsc
```

Un resultado 2 significa infracción del perfil; 3 significa contrato o entrada inválidos. Ninguno
permite continuar al bootstrap.

SH07 añade `verify_ir.py`: construye la matriz diferencial de HIR/MIR/CFG/dominancia/SSA, exige
estabilidad entre O0/O1/O2/O3 y materializa bajo `selfhost/out` un caso reducido cuando aparece la
primera divergencia estructural.

SH08 añade `verify_llvm_backend.py`: valida objetos, assembly, anatomía `llvm-readobj` y
ejecutables ya probados por CMake en cuatro perfiles y ambos linkers; publica hashes canónicos.
