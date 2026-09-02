import wy3d

def printElement(db, elemId):
    if db is None:
        return
    elem = db.getElement(elemId)
    if elem is None:
        return
    print("------------")
    print(f"className = {elem.getClassName()}")
    print(f"id = {elem.getId()}")
    print(f"database = {elem.getDatabase()}")
    print(f"isErased = {elem.isErased()}")
    print(f"isHidden = {elem.isHidden()}")
    print(f"owner = {elem.getParent()}")
    print(f"children = {elem.getChildren()}")

db = wy3d.getActiveDatabase()
for elemId in db:
    printElement(db, elemId)
