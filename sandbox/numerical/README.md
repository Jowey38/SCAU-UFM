# SCAU-UFM numerical sandbox

This sandbox is an isolated Python reference prototype for mixed-grid DPM / HLLC / source-term well-balanced evidence.

Validated local commands on 2026-05-12:

```bash
python sandbox/numerical/src/run_golden_sandbox.py
python -m unittest discover -s sandbox/numerical/tests -p "test_*.py"
```

The proof notebooks require the `sympy` and `notebook` dependencies declared in `pyproject.toml`. They are archived with executed outputs used by `evidence/well_balanced_proof.md` and `evidence/sandbox_report.md`.
