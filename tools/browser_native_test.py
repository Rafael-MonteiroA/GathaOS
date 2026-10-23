"""Run the same guest URL/HTML source with host memory/UB instrumentation."""
from pathlib import Path
import os
import subprocess
Path('build').mkdir(exist_ok=True)
# A fixed executable base avoids Clang 14 ASan startup collisions on WSL ASLR.
subprocess.run(['clang','-std=c11','-O1','-g','-no-pie','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-fno-omit-frame-pointer','tools/browser_native_test.c','apps/browser/document.c',
                'libgatha/url.c','libgatha/string.c','-o','build/browser-native-test'],check=True)
subprocess.run(['build/browser-native-test'],check=True,timeout=60,
               env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
