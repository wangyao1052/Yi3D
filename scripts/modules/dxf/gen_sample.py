import math
import os
import sys

import ezdxf

# Generate samples/dxf/sample.dxf covering every entity type the importer
# supports, plus a few deliberately unsupported ones. Run with any Python
# that has ezdxf installed:
#   python scripts/modules/dxf/gen_sample.py [output.dxf]

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
    "samples", "dxf", "sample.dxf")

doc = ezdxf.new("R2010", setup=True)
doc.layers.add("CONSTRUCTION", color=8)
doc.layers.add("CENTERLINE", color=9, linetype="CENTER")
msp = doc.modelspace()

# LINE on default layer
msp.add_line((0, 0, 0), (100, 0, 0))
# LINE on CONSTRUCTION layer
msp.add_line((0, 0, 0), (0, 100, 0), dxfattribs={"layer": "CONSTRUCTION"})
# LINE on CENTERLINE layer
msp.add_line((50, -20, 0), (50, 120, 0), dxfattribs={"layer": "CENTERLINE"})

# CIRCLE
msp.add_circle((0, 0, 0), 10)

# ARC: plain / crossing 0 deg / full 360 deg
msp.add_arc((60, 60, 0), 20, 30, 120)
msp.add_arc((60, 60, 0), 25, 300, 60)
msp.add_arc((60, 60, 0), 30, 0, 360)

# ELLIPSE: full / arc
msp.add_ellipse((100, 100, 0), (30, 0, 0), 0.5)
msp.add_ellipse((150, 100, 0), (30, 0, 0), 0.5,
                math.radians(30), math.radians(150))
# ELLIPSE arc: end < start (crossing 0 deg)
msp.add_ellipse((200, 150, 0), (20, 0, 0), 0.5,
                math.radians(300), math.radians(60))

# SPLINE: fit points / control points
msp.add_spline(fit_points=[(0, 150, 0), (50, 180, 0), (100, 150, 0), (150, 180, 0)])
spline = msp.add_spline()
spline.set_open_uniform([(0, 200, 0), (50, 230, 0), (100, 200, 0), (150, 230, 0)],
                        degree=3)

# LWPOLYLINE: open with line + bulge arc segments
msp.add_lwpolyline([(0, 250, 0, 0, 0.0),
                    (50, 250, 0, 0, 0.5),
                    (100, 250, 0, 0, 0.0)],
                   format="xyseb")
# LWPOLYLINE: closed with a negative bulge
pl = msp.add_lwpolyline([(0, 280, 0, 0, 0.0),
                         (50, 280, 0, 0, -0.5),
                         (50, 320, 0, 0, 0.0),
                         (0, 320, 0, 0, 0.0)],
                        format="xyseb")
pl.closed = True

# POINT
msp.add_point((10, 10, 0))

# Unsupported: TEXT must be skipped with a warning
msp.add_text("hello", dxfattribs={"insert": (120, 40, 0), "height": 5})

os.makedirs(os.path.dirname(OUT), exist_ok=True)
doc.saveas(OUT)
print("wrote", OUT)
