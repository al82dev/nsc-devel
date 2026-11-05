# Tanda 08: modo de lenguaje de records

El snapshot conserva si Clang analizó cada record como C++ o como C. Esta
distinción permite estudiar unions C sin atribuir automáticamente las mismas
reglas de vida a unions C++. El campo por sí solo no habilita accesos ni prueba
la representación o el ABI; está pendiente de validación agrupada.
