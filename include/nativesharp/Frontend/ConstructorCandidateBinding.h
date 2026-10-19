#pragma once

#include "nativesharp/Frontend/ConstructorOwnershipTransfer.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace nativesharp::frontend
{
struct ConstructorCandidateBinding final
{
    std::optional<std::size_t> memberOrdinal;
    bool implicitDefault = false;
    bool accessible = false;
    bool viable = false;
    std::vector<ProjectImplicitConversion> conversions;
    std::vector<std::size_t> argumentParameterOrdinals;
    std::vector<std::size_t> defaultedParameterOrdinals;
    std::vector<bool> parameterPackElements;
    std::optional<std::size_t> parameterPackOrdinal;
    bool usesExpandedParameterPack = false;
    bool synthesizesEmptyParameterPack = false;
    std::size_t defaultedParameterCount = 0;
    // NSC-LIMIT-010. Viabilidad de PROPIEDAD por argumento, en paralelo a `conversions`, que solo
    // lleva la fisica. Un candidato puede ser fisicamente valido y aun asi incumplir el contrato:
    // `new Caja(duenio)` sin `move` convierte perfectamente y sigue siendo ilegal.
    //
    // Se guarda el veredicto de CADA argumento y no un booleano por candidato porque el
    // diagnostico tiene que nombrar el argumento concreto y decir cual de los tres
    // incumplimientos fue.
    std::vector<OwnershipArgumentFit> ownershipFits;
    // Parallel to conversions/ownershipFits. True means the physical conversion exists but a
    // named move-only place would be copied into a by-value constructor parameter.
    std::vector<bool> moveOnlyCopyRejects;
    // Las transferencias que este candidato exigiria si saliera elegido.
    std::vector<ConstructorOwnershipTransfer> ownershipTransfers;
    // Argumentos que producen un string owned temporal prestado solo durante la construccion.
    // No son transferencias: HIR les crea un owner oculto y lo destruye al terminar la expresion.
    std::vector<std::size_t> fullExpressionBorrowArgumentOrdinals;
};
}
