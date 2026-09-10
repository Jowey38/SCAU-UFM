from pathlib import Path
import tempfile
import unittest
from scau_preproc import inp_io, swmm_author

class InpTests(unittest.TestCase):
    def test_whitelist_and_semantic_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            a=Path(d)/'a.inp'; b=Path(d)/'b.inp'
            text='[TITLE]\nhello\n[OPTIONS]\nFLOW_UNITS CMS\n[JUNCTIONS]\nJ1 1 2 0 0 0\n'
            a.write_text(text)
            inp_io.write_inp(inp_io.parse_inp(a), b)
            self.assertTrue(inp_io.semantic_equal(a,b))
    def test_forbidden_section(self):
        with self.assertRaises(ValueError): inp_io.parse_inp('[SUBCATCHMENTS]\nS1 S1 1')
    def test_author_network(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); geo=root/'network.geojson'; inp=root/'model.inp'
            geo.write_text('{"type":"FeatureCollection","drainage_network_schema_version":1,"features":['
                '{"type":"Feature","geometry":{"type":"Point","coordinates":[0,0]},"properties":{"element":"junction","node_id":"J1","invert_elevation_m":1}},'
                '{"type":"Feature","geometry":{"type":"Point","coordinates":[10,0]},"properties":{"element":"outfall","node_id":"O1","invert_elevation_m":0,"outfall_type":"FREE"}},'
                '{"type":"Feature","geometry":{"type":"LineString","coordinates":[[0,0],[10,0]]},"properties":{"element":"conduit","link_id":"C1","from_node":"J1","to_node":"O1","xsection":{"shape":"CIRCULAR","geom1_m":1}}}]}')
            result=swmm_author.author_network(geo, inp)
            self.assertEqual(result['mode'],'authored'); self.assertIn('[CONDUITS]', inp.read_text())

if __name__ == '__main__': unittest.main()
