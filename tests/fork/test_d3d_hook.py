"""Compatibility entrypoint for the current owned D3D installer contract.

The R02h test validated the historical PatchVtableSlot implementation and its
recorded evidence remains in planning/results/R02h-m1-d3d.md. That symbol was
removed by the focused installer, so current-source runs use the transaction
fixture, including the current pre-call static-template callback route.
"""

import argparse
import os
from pathlib import Path
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path)
    parser.add_argument("--msbuild", help="accepted for historical compatibility")
    args = parser.parse_args()
    current = Path(__file__).with_name("test_d3d_install_transaction.py")
    command = [sys.executable, str(current)]
    if args.build_root is not None:
        private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork/R02t/luna-test"
        current_private = Path(os.environ["LOCALAPPDATA"]) / "V2DLLFork/R02w/luna-tests"
        resolved = args.build_root.resolve()
        if private.resolve() in resolved.parents or current_private.resolve() in resolved.parents:
            command += ["--build-root", str(resolved)]
        else:
            print("Historical --build-root ignored; current run uses a new private D3D test directory.")
    return subprocess.call(command)


if __name__ == "__main__":
    raise SystemExit(main())
