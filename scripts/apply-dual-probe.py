"""Apply narrowly scoped diagnostics to pinned disposable west checkouts only."""
import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("workspace", type=Path)
parser.add_argument("--radio", action="store_true")
args = parser.parse_args()
root = args.workspace.resolve()
patches = Path(__file__).resolve().parent.parent / "patches"

def apply(project, revision, patch):
    path = root / project
    actual = subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()
    if actual != revision:
        raise SystemExit(f"Refusing {project} at unexpected revision {actual}")
    subprocess.run(["git", "-C", str(path), "apply", "--check", str(patches / patch)], check=True)
    subprocess.run(["git", "-C", str(path), "apply", str(patches / patch)], check=True)

apply("zmk-feature-split-esb", "314c7cbaf4a74e1add1d6ffc8249de3e29965b8c", "esb-timeslot-kconfig.patch")
if args.radio:
    apply("nrf", "9b3d2623fdcd9c0fd0284f860beea924568c9826", "nrf-esb-radio-owner.patch")
