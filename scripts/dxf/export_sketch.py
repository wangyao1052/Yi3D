import json
import os
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import wy3d  # noqa: E402

from dxf_export import export_dxf  # noqa: E402


def load_params():
    params = globals().get("__yi3d_params")
    if params:
        return params
    # Standalone fallback: first argv is a JSON file {"dxf_path": ..., "sketch_id": ...}
    if len(sys.argv) > 1:
        with open(sys.argv[1], encoding="utf-8") as f:
            return json.load(f)
    raise SystemExit("YI3D_DXF_ERROR: missing params")


def main():
    params = load_params()
    dxf_path = params["dxf_path"]
    sketch_id = int(params["sketch_id"])

    db = wy3d.getActiveDatabase()
    sketch = db.getElement(wy3d.ElementId(sketch_id))
    if sketch is None or type(sketch) is not wy3d.Sketch:
        raise SystemExit("YI3D_DXF_ERROR: sketch not found")

    export_dxf(dxf_path, sketch)
    print("YI3D_DXF: exported ok")


try:
    main()
except SystemExit:
    raise
except BaseException:
    traceback.print_exc()
    raise SystemExit("YI3D_DXF_ERROR: export failed")
