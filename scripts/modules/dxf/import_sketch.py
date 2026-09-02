import json
import os
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import wy3d  # noqa: E402

from dxf_import import import_dxf  # noqa: E402


def load_params():
    params = globals().get("__yi3d_params")
    if params:
        return params
    # Standalone fallback: first argv is a JSON file {"dxf_path": ...}
    if len(sys.argv) > 1:
        with open(sys.argv[1], encoding="utf-8") as f:
            return json.load(f)
    raise SystemExit("YI3D_DXF_ERROR: missing params")


def main():
    params = load_params()
    dxf_path = params["dxf_path"]

    db = wy3d.getActiveDatabase()
    tm = db.getTransactionManager()
    if tm.getActiveTransaction() is not None:
        raise SystemExit("YI3D_DXF_ERROR: another transaction is already active")

    # The script owns the whole transaction: create the sketch, fill it and
    # commit; on any failure the transaction is aborted, which rolls back
    # everything created in it
    trans = tm.startTransaction()
    if trans is None:
        raise SystemExit("YI3D_DXF_ERROR: cannot start transaction")

    try:
        # DXF world coordinates map 1:1 onto a sketch on the XY plane
        plane = wy3d.SketchPlane(
            wy3d.Vector3(0.0, 0.0, 0.0),
            wy3d.Vector3(0.0, 0.0, 1.0),
            wy3d.Vector3(1.0, 0.0, 0.0))
        sketch = wy3d.Sketch.create(trans, plane)
        if sketch is None:
            raise SystemExit("YI3D_DXF_ERROR: cannot create sketch")
        trans.addNewlyCreatedElement(sketch)

        stats = import_dxf(dxf_path, trans, sketch)
        tm.endTransaction()
        print("YI3D_DXF: imported=%d skipped=%d" % (stats.imported, len(stats.skipped)))
        for dxftype, reason in stats.skipped:
            print("YI3D_DXF: skip %s - %s" % (dxftype, reason))
    except BaseException:
        if tm.getActiveTransaction() is not None:
            tm.abortTransaction()
        raise


try:
    main()
except SystemExit:
    raise
except BaseException:
    traceback.print_exc()
    raise SystemExit("YI3D_DXF_ERROR: import failed")
