"""Exercise exception cleanup in polls when the finally body does not suspend."""

import sys

import async_poll_call_exception as cases


if __name__ == '__main__':
    selected = (
        'sync_control', 'async_call', 'async_call_empty_finally',
        'async_generator_call', 'generator_call_empty_finally',
        'async_generator_direct_throw', 'local_catch', 'generator_local_catch',
        'async_cleanup', 'async_cleanup_replacement', 'async_destructor_replacement',
        'generator_cleanup', 'generator_cleanup_nested', 'generator_ordered_cleanup',
    )
    cases.CASES = {name: cases.CASES[name] for name in selected}
    sys.exit(cases.main())
