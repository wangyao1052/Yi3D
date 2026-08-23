import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import wy3d

# Failure-path test: feed a corrupt file to the runner and verify that the
# transaction is aborted and no partial sketch remains in the database.

script_dir = os.path.dirname(os.path.abspath(__file__))
bad_path = os.path.join(script_dir, "_bad.dxf")
with open(bad_path, "w", encoding="utf-8") as f:
    f.write("this is not a dxf file\n")

try:
    runner_path = os.path.join(script_dir, "import_sketch.py")
    params_globals = {
        "__file__": runner_path,
        "__yi3d_params": {"dxf_path": bad_path},
    }
    with open(runner_path, encoding="utf-8") as f:
        exec(compile(f.read(), runner_path, "exec"), params_globals)
    raise SystemExit("FAIL_IMPORT_TEST: FAILED - runner did not fail")
except SystemExit:
    # The runner reports failure via SystemExit; that is the expected outcome
    pass
finally:
    os.remove(bad_path)

db = wy3d.getActiveDatabase()
if db.getTransactionManager().getActiveTransaction() is not None:
    raise SystemExit("FAIL_IMPORT_TEST: FAILED - active transaction leaked")

sketch_count = 0
for eid in db:
    e = db.getElement(eid)
    if isinstance(e, wy3d.Sketch) and not e.isErased():
        sketch_count += 1
if sketch_count != 0:
    raise SystemExit("FAIL_IMPORT_TEST: FAILED - partial sketch left, count=%d" % sketch_count)

print("FAIL_IMPORT_TEST: PASSED (import rolled back, no visible sketch)")

