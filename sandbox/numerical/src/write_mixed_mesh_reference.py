from __future__ import annotations

import json
from pathlib import Path

from mesh import build_mixed_minimal_mesh, to_reference_dict


output_path = Path(__file__).resolve().parents[1] / "cases" / "mixed_mesh_minimal.json"
output_path.write_text(
    json.dumps(to_reference_dict(build_mixed_minimal_mesh()), indent=2) + "\n",
    encoding="utf-8",
)
print(output_path)
