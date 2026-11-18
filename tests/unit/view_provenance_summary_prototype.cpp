#include "ViewProvenanceSummaryPrototype.h"
#include "ViewProvenanceContractPrototype.h"
#include <cassert>

using namespace nativesharp::frontend::view_provenance_prototype;

int main()
{
    Graph copy(2);
    const auto input = *copy.parameter(0);
    const auto tupleElement = copy.local();
    copy.transfer(input, tupleElement);
    copy.output(tupleElement, {SinkKind::Output, 1});
    const auto result = copy.finish(true);
    assert(result.verified && result.edges.size() == 1);
    assert(result.edges[0].sourceParameter == 0);
    assert(result.edges[0].sink.kind == SinkKind::Output && result.edges[0].sink.ordinal == 1);
    const auto partial = copy.finish(false);
    assert(!partial.verified && partial.edges.size() == 1);

    Graph unknown(1);
    unknown.parameter(0);
    assert(!unknown.finish(false).verified);
    unknown.unsupported();
    assert(!unknown.finish(true).verified);

    Graph cycle(2);
    const auto left = *cycle.parameter(0);
    const auto right = *cycle.parameter(1);
    const auto a = cycle.local();
    const auto b = cycle.local();
    cycle.transfer(left, a);
    cycle.transfer(right, b);
    cycle.transfer(a, b);
    cycle.transfer(b, a);
    cycle.output(a, {SinkKind::Return, 0});
    const auto joined = cycle.finish(true);
    assert(joined.verified && joined.edges.size() == 2);

    Graph invalid(1);
    const auto value = *invalid.parameter(0);
    invalid.output(value, {SinkKind::Output, 3});
    assert(!invalid.finish(true).verified);

    // A verified callee's output becomes a source in the forwarding caller.
    Graph caller(2);
    const auto callerInput = *caller.parameter(0);
    const auto callerOutput = *caller.parameter(1);
    caller.call(result, {callerInput, callerOutput}, std::nullopt);
    caller.output(callerOutput, {SinkKind::Output, 1});
    const auto forwarded = caller.finish(true);
    assert(forwarded.verified && forwarded.edges.size() == 2);
    assert(forwarded.edges[0].sourceParameter == 0 && forwarded.edges[0].sink.ordinal == 1);
    assert(forwarded.edges[1].sourceParameter == 1 && forwarded.edges[1].sink.ordinal == 1);

    Graph untrusted(2);
    const auto untrustedInput = *untrusted.parameter(0);
    const auto untrustedOutput = *untrusted.parameter(1);
    untrusted.call(partial, {untrustedInput, untrustedOutput}, std::nullopt);
    assert(!untrusted.finish(true).verified);

    Graph invalidCall(1);
    const auto only = *invalidCall.parameter(0);
    invalidCall.call(result, {only}, std::nullopt);
    assert(!invalidCall.finish(true).verified);
    Summary emptyTwoParameter;
    emptyTwoParameter.verified = true;
    emptyTwoParameter.parameterCount = 2;
    Graph wrongArity(1);
    const auto wrongArgument = *wrongArity.parameter(0);
    wrongArity.call(emptyTwoParameter, {wrongArgument}, std::nullopt);
    assert(!wrongArity.finish(true).verified);

    Graph missingReturn(1);
    const auto returnedInput = *missingReturn.parameter(0);
    Summary returns;
    returns.verified = true;
    returns.parameterCount = 1;
    returns.edges.push_back({0, {SinkKind::Return, 0}});
    missingReturn.call(returns, {returnedInput}, std::nullopt);
    assert(!missingReturn.finish(true).verified);

    const auto serialized = encodeContract(result, "Sink.Save");
    assert(serialized.has_value());
    const auto decoded = decodeContract(*serialized, 2, {false, true}, false, "Sink.Save");
    assert(decoded.has_value() && decoded->verified && decoded->edges.size() == 1);
    Graph binaryCaller(2);
    const auto binaryInput = *binaryCaller.parameter(0);
    const auto binaryOutput = *binaryCaller.parameter(1);
    binaryCaller.call(*decoded, {binaryInput, binaryOutput}, std::nullopt);
    binaryCaller.output(binaryOutput, {SinkKind::Output, 1});
    assert(binaryCaller.finish(true).edges.size() == 2);

    const auto absent = decodeContract({}, 2, {false, true}, false, "Sink.Save");
    assert(!absent.has_value());
    const auto encodedUnknown = encodeContract(partial, "Sink.Save");
    assert(encodedUnknown.has_value());
    const auto decodedUnknown = decodeContract(*encodedUnknown, 2, {false, true}, false, "Sink.Save");
    assert(decodedUnknown.has_value() && !decodedUnknown->verified);
    auto malformed = *serialized;
    malformed.pop_back();
    assert(!decodeContract(malformed, 2, {false, true}, false, "Sink.Save"));
    assert(!decodeContract(*serialized, 2, {false, false}, false, "Sink.Save"));
    assert(!decodeContract(*serialized, 1, {false}, false, "Sink.Save"));
    assert(!decodeContract(*serialized, 2, {false, true}, false, "Sink.Fake"));
    auto trailing = *serialized;
    trailing.push_back(0);
    assert(!decodeContract(trailing, 2, {false, true}, false, "Sink.Save"));
    auto duplicate = *serialized;
    duplicate[5 + std::string("Sink.Save").size()] = 2;
    duplicate.insert(duplicate.end(), serialized->end() - 3, serialized->end());
    assert(!decodeContract(duplicate, 2, {false, true}, false, "Sink.Save"));
    const auto returnBytes = encodeContract(returns, "Sink.Return");
    assert(returnBytes.has_value());
    assert(!decodeContract(*returnBytes, 1, {false}, false, "Sink.Return"));
    assert(decodeContract(*returnBytes, 1, {false}, true, "Sink.Return"));
    Graph unknownBinaryCaller(1);
    const auto unknownArgument = *unknownBinaryCaller.parameter(0);
    unknownBinaryCaller.call(*decodedUnknown, {unknownArgument}, std::nullopt);
    assert(!unknownBinaryCaller.finish(true).verified);
}
