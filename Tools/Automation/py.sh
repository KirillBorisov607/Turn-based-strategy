#!/bin/bash
# Usage: py.sh <script.py>  - runs the script inside the Unreal Editor and prints its stdout/traceback
SCRIPT=$(cygpath -m "$1")
OUT=C:/ProjectsUE/_tools/py_out.txt
rm -f /c/ProjectsUE/_tools/py_out.txt
CODE="import sys, io, traceback\n_b = io.StringIO()\n_o = sys.stdout\nsys.stdout = _b\ntry:\n    exec(open(r'$SCRIPT', encoding='utf-8').read(), {'__name__': '__main__'})\nexcept Exception:\n    traceback.print_exc(file=_b)\nfinally:\n    sys.stdout = _o\n    open(r'$OUT', 'w', encoding='utf-8').write(_b.getvalue())\n"
/c/ProjectsUE/_tools/mcp.sh execute_python "{\"code\":\"$CODE\"}" > /dev/null
cat /c/ProjectsUE/_tools/py_out.txt 2>/dev/null || echo "(no output file)"
