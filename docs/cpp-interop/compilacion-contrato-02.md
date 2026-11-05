# Compilacion contrato 02

Se registra una sección del cambio acumulado en `include/nativesharp/Frontend/ProjectCompilation.h`. Contexto: `class ProjectCompilation final`.

El propósito es conectar el análisis, la compilación o la publicación de referencias C++ en esta etapa. La sección se conserva tal como estaba en el archivo de trabajo; su validación corresponde al conjunto.

Puntos presentes en esta sección:

- `using CppFunctionProvider = std::function<bool(`
- `const compilation::ProjectGraph&, std::span<const interop::CppProjectSessions>,`
- `std::span<const std::unique_ptr<ProjectSyntaxUnit>>,`
- `std::span<const CppFunctionDemand>,`
- `std::vector<NamespaceFunctionSymbol>&,`
