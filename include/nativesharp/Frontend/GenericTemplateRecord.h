#pragma once

#include "nativesharp/Frontend/GenericTemplateAccessibility.h"
#include "nativesharp/Frontend/GenericTemplateExternalTypeReference.h"
#include "nativesharp/Frontend/GenericTemplateKind.h"

#include "nativesharp/AST/AstNode.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct GenericTemplateRecord final
{
    GenericTemplateKind kind = GenericTemplateKind::Class;
    GenericTemplateAccessibility accessibility = GenericTemplateAccessibility::Internal;
    const ast::AstNode* syntax = nullptr;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    std::string moduleName;
    std::string namespaceName;
    // NSC-LIMIT-009. La CADENA de tipos contenedores, unida con `+`: `A+Inner`, no `Inner`. Para
    // un metodo generico es su dueno; para un tipo generico, sus contenedores.
    std::string ownerName;
    std::string name;
    // El nombre del tipo con su cadena delante --`A+Inner`-- o el nombre a secas si no esta
    // anidado. Es lo que hace que dos `Inner<T>` bajo contenedores distintos no compartan clave.
    std::string qualifiedTypeName;
    std::vector<std::string> parameters;
    std::vector<ast::GenericParameterSyntax> parameterSyntaxes;
    // NSC-LIMIT-007. Las clausulas `where` DE LA PLANTILLA, copiadas del AST sin resolver. Se
    // guardan aqui y no se validan al recolectar porque cuando esta fase corre --la segunda de
    // toda la cadena-- todavia no existen el name lookup, la herencia ni las interfaces. Quien
    // valida es ProjectGenericConstraintModel, con las tres ya construidas.
    std::vector<ast::GenericConstraintClause> constraints;
    std::string canonicalKey;
    // NativeSharp 3.0 T04-F06. Methods enter one controlled-specialization family only when
    // their generic arity, receiver form and alpha-normalized value-parameter pattern agree.
    // Constraints are deliberately excluded: they define the partial order inside the family.
    std::string specializationFamilyKey;
    bool isControlledSpecialization = false;
    bool isControlledSpecializationFamily = false;
    // Canonical identities reached from this template's declaration/body. The producer uses
    // these edges to compute the reusable fixed-point closure; NSMETA persists them so artifact
    // verification can prove that every same-module dependency is present.
    std::vector<std::string> dependencies;
    // Ordinary named types reached from this template through its original using/alias scope.
    // These references let NSMETA make the declaration self-contained instead of depending on
    // source-level using directives that are intentionally not transported with a template body.
    std::vector<GenericTemplateExternalTypeReference> externalTypeReferences;

    // T05-F02. Source-level impl blocks merged into this template. They are retained separately
    // so NSMETA can publish the original class declaration followed by its coherence extensions;
    // a consumer then runs the same merge before monomorphization.
    std::vector<const ast::AstNode*> extensionSyntaxes;

    // Explicit generic uses that could not be resolved while inspecting the template. They are
    // harmless in an unreachable private template, but make a public reusable closure invalid.
    std::vector<std::string> unresolvedDependencies;

    // NSC305-C01. SHA-256 of the template's source text (reconstructed from the lexer tokens), the
    // C1 cache key input that invalidates on any source change (including the body).
    std::string sourceFingerprint;
    // NSC305-C01. Full C1 cache key: SHA-256 of (source fingerprint + cache schema version).
    std::string c1CacheKey;
};
}
