#pragma once

// T09-F04. A ref/ref-mut foreach lends element storage from its source for the whole loop.
// Keep that lexical loan active while the synthesized candidate block is analyzed so an
// assignment or mutating receiver call cannot invalidate the iterator behind the yielded
// reference. The place key is the original source, not the hidden foreach snapshot.
struct ActiveIterationBorrow final
{
    std::string sourcePlace;
    bool mutableElements = false;
};
