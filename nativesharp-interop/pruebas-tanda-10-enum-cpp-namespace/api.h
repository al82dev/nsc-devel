#pragma once
namespace First { enum Mode : int { Value = 10 }; struct Box { Mode field; int padding; }; }
namespace Second { enum Mode : int { Value = 20 }; struct Box { Mode field; int padding; }; }
namespace { enum Hidden : int { HiddenValue = 30 }; }
int enum_anchor();
