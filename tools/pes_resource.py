"""Load one PES 2013 base-archive resource by ID; never execute game code."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

if __package__:
    from . import afs_inspect, wesys_unpack
else:
    import afs_inspect
    import wesys_unpack

# Only these selectors were checked against the captured archive registry.
ARCHIVES = {0x0b: "dt0b.img", 0x0c: "dt0c.img", 0x0d: "dt0d.img", 0x0e: "dt0e.img"}


def load_resource(img_dir: Path | str, resource_id: int) -> tuple[bytes, dict]:
    """Resolve the checked base route, read exactly one entry and decode WESYS."""
    if type(resource_id) is not int or not 0 <= resource_id <= 0xffffffff:
        raise ValueError("resource ID must be an unsigned 32-bit integer")
    selector, entry = resource_id >> 24, resource_id & 0xffffff
    if selector not in ARCHIVES:
        raise ValueError(f"unsupported archive selector: 0x{selector:02x} (supported: 0x0b through 0x0e)")
    raw, archive = afs_inspect.read_afs_entry(
        Path(img_dir) / ARCHIVES[selector], entry, max_size=wesys_unpack.MAX_INPUT_SIZE)
    decoded, wesys = wesys_unpack.decode_wesys(raw)
    report = {"schema_version": 1, "format": "PES2013-resource", "route": "base_archive",
              "resource_id_hex": f"0x{resource_id:08x}", "archive_selector": selector,
              "archive": archive, "wesys": wesys}
    return decoded, report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--img-dir", required=True, type=Path, help="local game's img directory")
    parser.add_argument("--id", required=True, type=lambda value: int(value, 0),
                        dest="resource_id", help="unsigned resource ID, e.g. 0x0b000011")
    parser.add_argument("--output", type=Path, help="new decoded file; omit to validate only")
    parser.add_argument("--json", type=Path, help="new report path; default: stdout")
    args = parser.parse_args(argv)
    try:
        destinations = [p for p in (args.output, args.json) if p is not None]
        if len({p.resolve() for p in destinations}) != len(destinations):
            raise ValueError("decoded output and JSON report must differ")
        for destination in destinations:
            if destination.exists() or destination.is_symlink():
                raise ValueError(f"output already exists: {destination}")
            if not destination.parent.is_dir():
                raise ValueError(f"output directory does not exist: {destination.parent}")
        decoded, report = load_resource(args.img_dir, args.resource_id)
        if args.output is not None:
            with args.output.open("xb") as destination:
                destination.write(decoded)
            report["output_name"] = args.output.name
        data = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
        if args.json is not None:
            with args.json.open("x", encoding="utf-8") as destination:
                destination.write(data)
        else:
            print(data, end="")
        return 0
    except (OSError, ValueError) as exc:
        print(f"pes_resource: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
