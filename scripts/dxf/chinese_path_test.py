import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import wy3d

# Same full-flow import as sample_import_test but with a Chinese file name in
# the path, to exercise non-ASCII paths end-to-end

script_dir = os.path.dirname(os.path.abspath(__file__))
dxf_path = os.path.join(script_dir, "_样例.dxf")
sample_path = os.path.join(
    os.path.dirname(os.path.dirname(script_dir)), "samples", "dxf", "sample.dxf")
shutil.copyfile(sample_path, dxf_path)

try:
    runner_path = os.path.join(script_dir, "import_sketch.py")
    params_globals = {
        "__file__": runner_path,
        "__yi3d_params": {"dxf_path": dxf_path},
    }
    with open(runner_path, encoding="utf-8") as f:
        exec(compile(f.read(), runner_path, "exec"), params_globals)
finally:
    os.remove(dxf_path)

db = wy3d.getActiveDatabase()
sketches = [db.getElement(eid) for eid in db
            if isinstance(db.getElement(eid), wy3d.Sketch) and not db.getElement(eid).isErased()]
if len(sketches) != 1:
    raise SystemExit("CHINESE_PATH_TEST: FAILED - expected 1 sketch, got %d" % len(sketches))

count = 0
for _ in sketches[0]:
    count += 1
print("CHINESE_PATH_TEST: imported entities = %d" % count)
