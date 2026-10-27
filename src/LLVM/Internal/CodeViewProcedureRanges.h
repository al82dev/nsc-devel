#pragma once

#include <filesystem>
#include <string>

namespace nativesharp::llvm_backend
{
// ES: Escribe en un objeto COFF x64 los campos DbgStart y DbgEnd de cada S_GPROC32/S_LPROC32 (y sus formas _ID)
// de CodeView. LLVM los deja siempre en 0, y un depurador que los lee (Visual Studio, WinDbg, cualquiera que use
// DIA o los hijos FuncDebugStart/FuncDebugEnd de DbgHelp) no sabe donde termina el prologo ni donde empieza el
// epilogo. Se siguen las mismas reglas que MSVC en -O0, comprobadas con cl.exe:
// - DbgStart: donde empieza la primera linea distinta de la linea de apertura de la funcion, que es donde ya
//   estan guardados los parametros; nunca antes del fin del prologo de .pdata/.xdata (SizeOfProlog);
// - DbgEnd: donde empieza el ultimo epilogo del cuerpo principal (sin funclets).
// EN: Writes the CodeView DbgStart and DbgEnd fields of every S_GPROC32/S_LPROC32 (and their _ID forms) into an
// x64 COFF object. LLVM always leaves them at 0, and a debugger that reads them (Visual Studio, WinDbg, anyone
// using DIA or DbgHelp's FuncDebugStart/FuncDebugEnd children) cannot tell where the prologue ends or where the
// epilogue begins. It follows the same rules as MSVC at -O0, checked against cl.exe:
// - DbgStart: where the first line other than the function's opening line starts, which is where the
//   parameters are already homed; never before the end of the .pdata/.xdata prologue (SizeOfProlog);
// - DbgEnd: where the last epilogue of the main body (without funclets) starts.
class CodeViewProcedureRanges final
{
public:
    static bool apply(const std::filesystem::path& objectPath, std::string& error);
};
}
