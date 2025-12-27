#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version-file", required=True)
    ap.add_argument("--image-url", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    ver = Path(args.version_file).read_text(encoding="utf-8").strip()
    out = Path(args.out)
    out.write_text(json.dumps({"version": ver, "image_url": args.image_url}, indent=2) + "\n", encoding="utf-8")
    print(f"yaotau: wrote {out} (version={ver})")

if __name__ == "__main__":
    main()
