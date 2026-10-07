# Tanda universal 22: marcador de valores move-only sin edición

## SCOPE LOCK

- Objetivo autorizado: eliminar toda dependencia funcional de Language 2/3 sin cambiar los contratos reales del
  lenguaje.
- Naturaleza: corrección funcional de compatibilidad acumulativa.
- Comportamiento que permanece: variants y opaque types son move-only salvo que declaren `copy`; el análisis de
  ownership, layout, ABI, lowering y diagnósticos no relacionados no cambia.
- Revisión conocida: `bc6d41637`; la batería completa ejecutó 684 CTests y dejó 92 fallos después de cerrar el
  contrato universal de bitfields.
- Rango evaluado: `bc6d41637..HEAD`.
- Archivos permitidos: productores y consumidor del marcador interno de valor move-only, más este documento.
- Archivos protegidos: runtime, bibliotecas, bindings, NSLink, NSDBG, NSASM, VS Code, instalaciones y toda unidad
  `C:`.
- Pruebas protegidas: no se modifican pruebas ni fixtures en esta tanda.
- Acciones prohibidas: deploy, escritura en `C:`, push, baseline, promoción, instalación y cambios oportunistas.
- Criterio de terminación: no queda ningún marcador `ns3-value`; la decisión se expresa únicamente mediante el
  contrato semántico `move-only-value`; la batería O0 completa no presenta regresiones nuevas.

## Causa raíz

La eliminación de `LanguageEdition` retiró la edición de proyectos y NSMETA, pero dejó un marcador interno llamado
`ns3-value`. `ProjectMemberModel::typeIsImplicitlyCopyable` todavía consultaba ese nombre para decidir si variants,
opaque types y wrappers de opaque results eran move-only. La conducta era semánticamente válida, pero seguía
representada como una propiedad de una versión retirada.

## Cambios

- `Parser::parseVariantDeclaration` emite `move-only-value` para variants.
- `Parser::parseOpaqueTypeDeclaration` emite `move-only-value` y conserva el opt-in `copy`.
- `ProjectOpaqueResultModel` aplica el mismo marcador a wrappers sintéticos.
- `ProjectMemberModel::typeIsImplicitlyCopyable` consulta el contrato semántico y deja de nombrar una edición.

No se cambia el algoritmo: el análisis continúa usando una worklist iterativa, con tiempo lineal respecto del grafo
de campos visitado y memoria lineal en el peor caso. No se añade recursión.

## Validación prevista

Después del commit se reconstruirán los consumidores afectados y se ejecutará la batería completa O0 con NSLink.
No se ejecutará baseline certificado.
