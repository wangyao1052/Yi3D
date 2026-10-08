import os
import shutil

import wy3d

# Imports the same solid twice: once from an ASCII file name (control) and
# once from a Chinese file name, exercising the Python str <-> C++
# std::wstring conversion of the ImportedSolid bindings end-to-end.

script_dir = os.path.dirname(os.path.abspath(__file__))
src_path = os.path.join(script_dir, "sample_box.step")
ascii_path = os.path.join(script_dir, "_ascii_control.step")
cn_path = os.path.join(script_dir, "_样例_模型.step")
cn_copy_path = os.path.join(script_dir, "_样例_模型_副本.step")

shutil.copyfile(src_path, ascii_path)
shutil.copyfile(src_path, cn_path)
shutil.copyfile(src_path, cn_copy_path)

try:
    db = wy3d.getActiveDatabase()
    trans_mgr = db.getTransactionManager()

    # Control: the fixture itself must import over an ASCII path.
    if not wy3d.ImportedSolid.isValidFilePath(ascii_path):
        raise SystemExit("IMPORT_CHINESE_PATH: FAILED - ascii control rejected")
    trans = trans_mgr.startTransaction()
    control = wy3d.ImportedSolid.create(transaction=trans, filePath=ascii_path)
    if control is None:
        trans_mgr.abortTransaction()
        raise SystemExit("IMPORT_CHINESE_PATH: FAILED - ascii control import returned None")
    trans_mgr.endTransaction()

    # Chinese file name: isValidFilePath / create / getFilePath round-trip.
    if not wy3d.ImportedSolid.isValidFilePath(cn_path):
        raise SystemExit("IMPORT_CHINESE_PATH: FAILED - isValidFilePath rejected %s" % cn_path)
    trans = trans_mgr.startTransaction()
    solid = wy3d.ImportedSolid.create(transaction=trans, filePath=cn_path)
    if solid is None:
        trans_mgr.abortTransaction()
        raise SystemExit("IMPORT_CHINESE_PATH: FAILED - import returned None for %s" % cn_path)
    got = solid.getFilePath()
    if got != cn_path:
        trans_mgr.abortTransaction()
        raise SystemExit("IMPORT_CHINESE_PATH: FAILED - getFilePath %r != %r" % (got, cn_path))

    # setFilePath to a second Chinese name must stick (round-trip again).
    status = solid.setFilePath(cn_copy_path)
    if status != wy3d.ErrorStatus.Ok:
        trans_mgr.abortTransaction()
        raise SystemExit("IMPORT_CHINESE_PATH: FAILED - setFilePath returned %s" % status)
    got = solid.getFilePath()
    if got != cn_copy_path:
        trans_mgr.abortTransaction()
        raise SystemExit("IMPORT_CHINESE_PATH: FAILED - setFilePath round-trip %r != %r" % (got, cn_copy_path))
    trans_mgr.endTransaction()

    imported = 0
    for eid in db:
        element = db.getElement(eid)
        if isinstance(element, wy3d.ImportedSolid) and not element.isErased():
            imported += 1
    if imported != 2:
        raise SystemExit("IMPORT_CHINESE_PATH: FAILED - expected 2 imported solids, got %d" % imported)

    print("IMPORT_CHINESE_PATH: PASSED - %s round-tripped" % got)
finally:
    os.remove(ascii_path)
    os.remove(cn_path)
    os.remove(cn_copy_path)
