import wy3d

# 获取当前选择集
ss = wy3d.getSelectionSet()
print(f"count = {ss.getCount()}")

# 遍历当前选择集
for sel in ss:
    elemId = sel.getElementId()
    print(f"elementId = {elemId}")