# mesh.py 原型证据

**日期**: 2026-05-11
**对应规范**: `superpowers/specs/2026-05-08-numerical-sandbox-design.md`

## 覆盖范围

- 已创建 `sandbox/numerical/src/mesh.py`，支持 `CellType.Triangle` 与 `CellType.Quadrilateral`。
- `Cell` 通过 `node_count` 与 `edge_count` 控制几何遍历，不假设所有单元为四边形。
- 已实现最小混合网格：C0 为四边形，C1/C2 为三角形，包含四边邻三角关键边 C0-C1。
- 已实现 quad-only 与 tri-only 对照网格构造函数。
- 已由 `mesh.py` 计算单元面积、中心、边长、边中点、边法向与邻接关系，并生成 `sandbox/numerical/cases/mixed_mesh_minimal.json`。

## 验证命令

```bash
python sandbox/numerical/src/write_mixed_mesh_reference.py && python - <<'PY'
import sys
sys.path.insert(0, 'sandbox/numerical/src')
from mesh import Cell, CellType, EdgeSpec, Node, build_mesh, build_mixed_minimal_mesh, build_quad_only_control_mesh, build_tri_only_control_mesh

def assert_close(actual, expected, tolerance=1e-15):
    assert abs(actual - expected) < tolerance, (actual, expected)

mixed = build_mixed_minimal_mesh()
assert len(mixed.nodes) == 6
assert [cell.cell_type for cell in mixed.cells] == [CellType.Quadrilateral, CellType.Triangle, CellType.Triangle]
edges = {edge.id: edge for edge in mixed.edges}
assert list(edges) == ['E0', 'E1', 'E2', 'E3', 'E4', 'E5', 'E6', 'E7']
assert edges['E0'].node_ids == ('N0', 'N1') and edges['E0'].left_cell is None and edges['E0'].right_cell == 'C0'
assert edges['E1'].node_ids == ('N1', 'N4') and edges['E1'].left_cell == 'C0' and edges['E1'].right_cell == 'C1'
assert edges['E2'].node_ids == ('N1', 'N2') and edges['E2'].left_cell is None and edges['E2'].right_cell == 'C1'
assert edges['E3'].node_ids == ('N2', 'N4') and edges['E3'].left_cell == 'C1' and edges['E3'].right_cell == 'C2'
assert edges['E4'].node_ids == ('N4', 'N3') and edges['E4'].left_cell == 'C0' and edges['E4'].right_cell is None
assert edges['E5'].node_ids == ('N0', 'N3') and edges['E5'].left_cell is None and edges['E5'].right_cell == 'C0'
assert edges['E6'].node_ids == ('N2', 'N5') and edges['E6'].left_cell is None and edges['E6'].right_cell == 'C2'
assert edges['E7'].node_ids == ('N5', 'N4') and edges['E7'].left_cell == 'C2' and edges['E7'].right_cell is None
assert_close(edges['E0'].normal[0], 0.0); assert_close(edges['E0'].normal[1], 1.0)
assert_close(edges['E1'].normal[0], 1.0); assert_close(edges['E1'].normal[1], 0.0)
assert_close(edges['E2'].normal[0], 0.0); assert_close(edges['E2'].normal[1], 1.0)
assert_close(edges['E3'].normal[0], 2 ** -0.5); assert_close(edges['E3'].normal[1], 2 ** -0.5)
assert_close(edges['E4'].normal[0], 0.0); assert_close(edges['E4'].normal[1], 1.0)
assert_close(edges['E5'].normal[0], 1.0); assert_close(edges['E5'].normal[1], 0.0)
assert_close(edges['E6'].normal[0], -1.0); assert_close(edges['E6'].normal[1], 0.0)
assert_close(edges['E7'].normal[0], 0.0); assert_close(edges['E7'].normal[1], 1.0)
assert len(build_quad_only_control_mesh().cells) == 2
assert len(build_tri_only_control_mesh().cells) == 4

def expect_value_error(edge_specs):
    try:
        build_mesh(
            (Node('N0', 0.0, 0.0), Node('N1', 1.0, 0.0), Node('N3', 0.0, 1.0)),
            (Cell('C0', CellType.Triangle, ('N0', 'N1', 'N3')),),
            edge_specs,
        )
        raise AssertionError('invalid edge specs must fail')
    except ValueError:
        pass

expect_value_error((EdgeSpec('E0', ('N1', 'N0'), 'C0', None), EdgeSpec('E1', ('N1', 'N3'), 'C0', None), EdgeSpec('E2', ('N3', 'N0'), 'C0', None)))
expect_value_error((EdgeSpec('E0', ('N0', 'N1'), 'C0', None), EdgeSpec('E0', ('N1', 'N3'), 'C0', None), EdgeSpec('E2', ('N3', 'N0'), 'C0', None)))
expect_value_error((EdgeSpec('E0', ('N0', 'N1'), 'C0', None), EdgeSpec('E1', ('N1', 'N3'), 'C0', None)))
expect_value_error((EdgeSpec('E0', ('N0', 'N1'), 'C0', None), EdgeSpec('E1', ('N1', 'N3'), 'C0', None), EdgeSpec('E2', ('N3', 'N0'), 'C0', None), EdgeSpec('E3', ('N0', 'N3'), 'C0', None)))
expect_value_error((EdgeSpec('E0', ('N0', 'N1', 'N3'), 'C0', None), EdgeSpec('E1', ('N1', 'N3'), 'C0', None), EdgeSpec('E2', ('N3', 'N0'), 'C0', None)))
print('mesh.py prototype checks passed')
PY
```

## 验证结果

```text
H:\githubcode\SCAU-UFM\sandbox\numerical\cases\mixed_mesh_minimal.json
mesh.py prototype checks passed
```

## 尚未覆盖

本证据只证明 `mesh.py` 原型可用；不代表 sandbox B 完整退出。完整退出仍需完成 sympy notebook、G1/G2/G3 1000 step、E1 抵消诊断和 `evidence/sandbox_report.md`。
