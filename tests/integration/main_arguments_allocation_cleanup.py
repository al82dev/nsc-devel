import subprocess
import sys
import tempfile
from pathlib import Path


def run(command, cwd):
    result = subprocess.run([str(x) for x in command], cwd=cwd,
                            capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{result.stdout}\n{result.stderr}')


def main():
    fixture = Path(sys.argv[1]).resolve()
    source = Path(__file__).with_name('main_arguments_entry_stubs.cpp').resolve()
    actual_source = Path(__file__).with_name('main_arguments_actual_fill_stubs.cpp').resolve()
    with tempfile.TemporaryDirectory(prefix='nsc-main-args-alloc-') as temporary:
        root = Path(temporary)
        entry = root / 'entry.obj'
        run([fixture, entry], root)
        for scenario in range(8):
            stub = root / f'stub{scenario}.obj'
            executable = root / f'case{scenario}.exe'
            run(['clang-cl', '/c', '/nologo', '/GS-', f'/DCASE={scenario}',
                 '/Fo' + str(stub), source], root)
            run(['nslink.exe', '--link', entry, stub, '--entry', '__nativesharp_start',
                 '--import', 'ucrtbase.dll:__C_specific_handler',
                 '--import', 'kernel32.dll:GetCurrentProcess,TerminateProcess,GetStdHandle,WriteFile,SetUnhandledExceptionFilter,RaiseException',
                 '-o', executable], root)
            if scenario == 7:
                result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                        text=True, timeout=60)
                if result.returncode != 0xE04E5301 or result.stdout != '11111111':
                    raise RuntimeError(f'unhandled exception: {result.returncode}, {result.stdout!r}')
                print('OBSERVED: unhandled exception reaches the OS filter before argument cleanup', flush=True)
            else:
                run([executable], root)
                print(f'PASS: entry allocation scenario {scenario}', flush=True)
        support = root / 'support.ll'
        extracted = root / 'fill.ll'
        fill = root / 'fill.obj'
        run([fixture, support, 'support'], root)
        run(['llvm-extract', '-S', '--func=__nativesharp_environment_get_command_line_args',
             '--func=__nativesharp_environment_get_command_line_arg_count',
             support, '-o', extracted], root)
        run(['clang', '-c', extracted, '-o', fill], root)
        for scenario in range(17):
            stub = root / f'actual{scenario}.obj'
            executable = root / f'actual{scenario}.exe'
            run(['clang-cl', '/c', '/nologo', '/GS-', f'/DCASE={scenario}',
                 '/Fo' + str(stub), actual_source], root)
            run(['nslink.exe', '--link', entry, fill, stub, '--entry', '__nativesharp_start',
                 '--import', 'ucrtbase.dll:__C_specific_handler',
                 '--import', 'kernel32.dll:GetCurrentProcess,TerminateProcess',
                 '-o', executable], root)
            run([executable], root)
            print(f'PASS: real argument conversion scenario {scenario}', flush=True)


if __name__ == '__main__':
    main()
