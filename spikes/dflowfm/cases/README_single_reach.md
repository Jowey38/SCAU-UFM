# single_reach.mdu skeleton

## Purpose

This file is the smallest repository-owned D-Flow FM case entry point for the
first real G11 runtime spike.

## What it is

- an author-authored skeleton;
- a stable filename and layout target for the first real runtime pass;
- a placeholder for upstream constant discharge and downstream constant stage
  references.

## What it is not

- not a validated hydraulic case;
- not evidence that G11 is implemented;
- not a GoldenSuite-ready runtime artifact.

## Still required before first real run

- the real D-Flow FM BMI DLL / shared library;
- the referenced companion file(s), such as `single_reach_net.nc`, if the final
  `.mdu` layout still uses that name;
- any boundary/ext companion files required by the chosen runtime setup.

## First-run evidence target

The first successful use of this skeleton should support only:
- initialize;
- 100-step advance;
- variable inventory capture;
- trace summary capture.
