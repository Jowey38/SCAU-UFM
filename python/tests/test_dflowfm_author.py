import json
import tempfile
import unittest
from pathlib import Path

from scau_preproc import dflowfm_author


class DFlowAuthorTests(unittest.TestCase):
    def sketch(self):
        return {"type": "FeatureCollection", "river_sketch_schema_version": 1, "features": [
            {"type": "Feature", "geometry": {"type": "Point", "coordinates": [0, 0]}, "properties": {"element": "river_node", "node_id": "A"}},
            {"type": "Feature", "geometry": {"type": "Point", "coordinates": [10, 0]}, "properties": {"element": "river_node", "node_id": "B"}},
            {"type": "Feature", "geometry": {"type": "LineString", "coordinates": [[0, 0], [10, 0]]}, "properties": {"element": "river_branch", "branch_id": "R", "from_node": "A", "to_node": "B"}},
        ]}

    def test_validation_rejects_isolated_node(self):
        data = self.sketch(); data["features"].insert(1, {"type": "Feature", "geometry": {"type": "Point", "coordinates": [5, 5]}, "properties": {"element": "river_node", "node_id": "C"}})
        with self.assertRaises(ValueError): dflowfm_author.validate_sketch(data)

    def test_deterministic_roundtrip_and_fail_closed_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); source = root / "river_sketch.geojson"; source.write_text(json.dumps(self.sketch(), sort_keys=True), encoding="utf-8")
            one = root / "one"; two = root / "two"
            a = dflowfm_author.author_river(source, one); b = dflowfm_author.author_river(source, two)
            self.assertEqual((one / "river_net.nc").read_bytes(), (two / "river_net.nc").read_bytes())
            self.assertEqual(a["provider_required"], b["provider_required"])
            self.assertFalse(a["export_readiness"]["exportable"])
            network = dflowfm_author.validate_ugrid(one / "river_net.nc")
            self.assertEqual(network["node_ids"], ["A", "B"])
            self.assertEqual(network["branch_ids"], ["R"])


if __name__ == "__main__": unittest.main()
