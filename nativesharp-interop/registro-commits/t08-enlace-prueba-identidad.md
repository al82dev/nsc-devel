# Enlace de la prueba de identidad

La batería en cb60b585e se detuvo al enlazar el objetivo de prueba de
funciones de namespace por la referencia `PrecompiledSemanticSnapshotCodec::decode`
desde `nsc_frontend`. Se añade `nsc_precompiled`, que contiene esa definición,
solo a ese objetivo. La batería debe repetirse.
