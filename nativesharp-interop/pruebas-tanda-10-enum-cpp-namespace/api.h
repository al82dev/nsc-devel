#pragma once
namespace First { enum Mode : int { Value = 10 }; }
namespace Second { enum Mode : int { Value = 20 }; }
namespace { enum Hidden : int { HiddenValue = 30 }; }
int enum_anchor();
