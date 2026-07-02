#!/usr/bin/env python3
"""
Preview and inspect PVP map JSON files.

Examples:
  python3 tools/pvp_map_json_tool.py preview --map pvp_office_panic --scope game --output /tmp/office_game.png
  python3 tools/pvp_map_json_tool.py preview --map pvp_office_panic --scope full --output /tmp/office_full.png
  python3 tools/pvp_map_json_tool.py summary --map pvp_office_panic

The game uses battleViewRect/deployViewRect from JSON for row/column mapping.
Use --scope game to preview the exact in-game mapping, and --scope full to
spread the same row/column count across the complete cropped map image.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Tuple


ROOT = Path(__file__).resolve().parents[1]
MAP_DIR = ROOT / "assets" / "maps"
ART_DIR = ROOT / "assets" / "ui" / "artwork"

COLORS = {
    "BLOCKED": (35, 35, 35, 145),
    "HIGH_GROUND": (255, 198, 0, 110),
    "PATH_A": (160, 32, 220, 150),
    "PATH_B": (235, 34, 22, 150),
    "PATH_SHARED": (255, 128, 0, 160),
    "SPAWN_A": (80, 80, 255, 170),
    "SPAWN_B": (255, 80, 80, 170),
    "CORE_A": (40, 80, 255, 190),
    "CORE_B": (255, 40, 40, 190),
    "DEPLOY_NEUTRAL": (76, 222, 76, 60),
}


def require_pillow():
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError as exc:
        raise SystemExit("This command needs Pillow: python3 -m pip install Pillow") from exc
    return Image, ImageDraw, ImageFont


def load_map(map_id: str) -> Dict[str, Any]:
    path = MAP_DIR / f"{map_id}.json"
    if not path.exists():
        raise SystemExit(f"Map JSON not found: {path}")
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def rect_from_json(value: Dict[str, Any], fallback: Tuple[int, int, int, int]) -> Tuple[int, int, int, int]:
    if not value:
        return fallback
    width = int(value.get("width", 0))
    height = int(value.get("height", 0))
    if width <= 0 or height <= 0:
        return fallback
    return int(value.get("x", 0)), int(value.get("y", 0)), width, height


def point_list(points: Iterable[Dict[str, int]]) -> List[Tuple[int, int]]:
    return [(int(point["row"]), int(point["col"])) for point in points]


def print_summary(config: Dict[str, Any]) -> None:
    grid = config["grid"]
    points = config.get("points", {})
    routes = config.get("routes", {})
    print(f"{config.get('name')} ({config.get('displayName', '')})")
    print(f"  image: {config.get('image')}")
    print(f"  grid: {grid.get('rows')} x {grid.get('cols')}")
    print(f"  imageCrop: {config.get('imageCrop')}")
    print(f"  battleViewRect: {config.get('battleViewRect')}")
    print(f"  deployViewRect: {config.get('deployViewRect')}")
    for key in ("SPAWN_A", "CORE_A", "SPAWN_B", "CORE_B"):
        print(f"  {key}: {point_list(points.get(key, []))}")
    for key in ("A", "B"):
        route_list = routes.get(key, [])
        if not route_list:
            print(f"  route {key}: []")
            continue
        path = point_list(route_list[0].get("path", []))
        print(f"  route {key} ({len(path)} points): {path}")


def preview(config: Dict[str, Any],
            output: Path,
            scope: str,
            show_legend: bool,
            draw_tile_fill: bool) -> None:
    Image, ImageDraw, ImageFont = require_pillow()
    image_path = ART_DIR / config["image"]
    if not image_path.exists():
        raise SystemExit(f"Map image not found: {image_path}")

    map_image = Image.open(image_path).convert("RGBA")
    crop = rect_from_json(config.get("imageCrop", {}), (0, 96, 1672, 604))
    crop_box = (crop[0], crop[1], crop[0] + crop[2], crop[1] + crop[3])
    cropped_map = map_image.crop(crop_box)

    canvas = Image.new("RGBA", (1672, 941), (0, 0, 0, 0))
    hud = ART_DIR / "battle_pvp_hud_clean.png"
    if hud.exists():
        canvas.alpha_composite(Image.open(hud).convert("RGBA"), (0, 0))
    if (ART_DIR / "battle_pvp.png").exists():
        pvp_frame = Image.open(ART_DIR / "battle_pvp.png").convert("RGBA")
        canvas.alpha_composite(pvp_frame.crop((0, 690, 1672, 941)), (0, 690))
    canvas.alpha_composite(cropped_map.resize((1672, 604)), (0, 96))

    rows = int(config["grid"]["rows"])
    cols = int(config["grid"]["cols"])
    if scope == "game":
        grid_rect = rect_from_json(config.get("battleViewRect", {}), (0, 96, 1672, 604))
    else:
        grid_rect = (0, 96, 1672, 604)

    draw = ImageDraw.Draw(canvas, "RGBA")
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial.ttf", 13)
        small = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial.ttf", 10)
    except OSError:
        font = small = None

    x0, y0, width, height = grid_rect
    cell_w = width / cols
    cell_h = height / rows
    tile_types = {(int(tile["row"]), int(tile["col"])): tile.get("type", "EMPTY")
                  for tile in config.get("tiles", [])}

    for row in range(rows):
        for col in range(cols):
            x = x0 + col * cell_w
            y = y0 + row * cell_h
            tile_type = tile_types.get((row, col), "EMPTY")
            if draw_tile_fill and tile_type in COLORS:
                draw.rectangle((x, y, x + cell_w, y + cell_h), fill=COLORS[tile_type])
            draw.rectangle((x, y, x + cell_w, y + cell_h), outline=(20, 75, 110, 140), width=1)
            draw.text((x + 2, y + 2), f"{row},{col}", fill=(20, 20, 20, 220), font=small)

    def center(point: Tuple[int, int]) -> Tuple[float, float]:
        row, col = point
        return x0 + (col + 0.5) * cell_w, y0 + (row + 0.5) * cell_h

    for key, color in (("A", (160, 0, 215, 255)), ("B", (230, 0, 0, 255))):
        route_list = config.get("routes", {}).get(key, [])
        if not route_list:
            continue
        path = point_list(route_list[0].get("path", []))
        points = [center(point) for point in path]
        if len(points) > 1:
            draw.line(points, fill=color, width=8, joint="curve")
        for index, point in enumerate(points):
            radius = 10 if index in (0, len(points) - 1) else 5
            draw.ellipse((point[0] - radius, point[1] - radius,
                          point[0] + radius, point[1] + radius), fill=color)

    draw.text((20, 705), f"{config.get('name')} | {scope} scope | {rows} x {cols}",
              fill=(45, 28, 14, 255), font=font)
    if show_legend and draw_tile_fill:
        left = 20
        top = 735
        for index, (tile_type, color) in enumerate(COLORS.items()):
            x = left + (index // 5) * 220
            y = top + (index % 5) * 28
            draw.rectangle((x, y, x + 24, y + 18), fill=color, outline=(0, 0, 0, 180))
            draw.text((x + 30, y), tile_type, fill=(45, 28, 14, 255), font=font)

    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output)
    print(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    summary_parser = subparsers.add_parser("summary")
    summary_parser.add_argument("--map", required=True, help="Map id, for example pvp_office_panic")

    preview_parser = subparsers.add_parser("preview")
    preview_parser.add_argument("--map", required=True, help="Map id, for example pvp_office_panic")
    preview_parser.add_argument("--scope", choices=("game", "full"), default="game")
    preview_parser.add_argument("--output", required=True)
    preview_parser.add_argument("--no-legend", action="store_true")
    preview_parser.add_argument("--no-tile-fill", action="store_true",
                                help="Draw only grid lines, coordinates, and routes.")

    args = parser.parse_args()
    config = load_map(args.map)
    if args.command == "summary":
        print_summary(config)
    elif args.command == "preview":
        preview(config,
                Path(args.output),
                args.scope,
                not args.no_legend,
                not args.no_tile_fill)


if __name__ == "__main__":
    main()
