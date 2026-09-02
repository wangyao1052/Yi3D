import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "modules", "dxf"))

import wy3d

# Full-flow test: the runner script owns the transaction, creates the XY
# sketch, fills it and commits. Emulate the C++ param injection and verify
# the resulting database state afterwards.

script_dir = os.path.dirname(os.path.abspath(__file__))
sample_path = os.path.join(script_dir, "..", "..", "modules", "dxf", "sample.dxf")

runner_path = os.path.join(script_dir, "..", "..", "modules", "dxf", "import_sketch.py")
params_globals = {
    "__file__": runner_path,
    "__yi3d_params": {"dxf_path": sample_path},
}
with open(runner_path, encoding="utf-8") as f:
    exec(compile(f.read(), runner_path, "exec"), params_globals)

db = wy3d.getActiveDatabase()
if db.getTransactionManager().getActiveTransaction() is not None:
    raise SystemExit("SAMPLE_IMPORT_TEST: FAILED - active transaction leaked")

sketches = [db.getElement(eid) for eid in db
            if isinstance(db.getElement(eid), wy3d.Sketch) and not db.getElement(eid).isErased()]
if len(sketches) != 1:
    raise SystemExit("SAMPLE_IMPORT_TEST: FAILED - expected 1 sketch, got %d" % len(sketches))

counts = {}
for entity_id in sketches[0]:
    entity = sketches[0].getDatabase().getElement(entity_id)
    name = entity.getClassName()
    counts[name] = counts.get(name, 0) + 1

print("SAMPLE_IMPORT_TEST: entity counts = %s" % counts)
