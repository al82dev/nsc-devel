# T08-004: forma IR de punteros en Clang

El control inicial encontró que Clang genera operaciones atómicas ptr,
mientras NSC usa i64 y conversiones de puntero. El verificador admite ambas
representaciones conservando la exigencia de orden y alineación.
