#!/usr/bin/env python3
"""Generate reproducible PVP trial map configs and preview overlays."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Set, Tuple

try:
    from PIL import Image, ImageDraw
except ImportError:  # pragma: no cover - preview is optional at runtime
    Image = None
    ImageDraw = None


ROWS = 10
COLS = 28
Point = Tuple[int, int]


def point(row: int, col: int) -> Dict[str, int]:
    return {"row": row, "col": col}


def rotate_180(pos: Point) -> Point:
    row, col = pos
    return ROWS - 1 - row, COLS - 1 - col


def rotate_route(route: Sequence[Point]) -> List[Point]:
    return [rotate_180(pos) for pos in route]


def expand_rect(rows: Iterable[int], cols: Iterable[int]) -> Set[Point]:
    return {(row, col) for row in rows for col in cols}


def unique_sorted(points: Iterable[Point]) -> List[Point]:
    return sorted(set(points), key=lambda item: (item[0], item[1]))


def assert_in_bounds(points: Iterable[Point], label: str) -> None:
    for row, col in points:
        if row < 0 or row >= ROWS or col < 0 or col >= COLS:
            raise ValueError(f"{label} contains out-of-bounds point {(row, col)}")


def route_to_json(route_id: str, route: Sequence[Point], spawn: Point, core: Point) -> Dict[str, object]:
    return {
        "id": route_id,
        "spawn": point(*spawn),
        "core": point(*core),
        "path": [point(row, col) for row, col in route],
    }


def build_sunny_beach_template() -> Dict[str, object]:
    spawn_a = (1, 13)
    core_a = (5, 1)
    spawn_b = rotate_180(spawn_a)
    core_b = rotate_180(core_a)

    route_a_upper: List[Point] = [
        (1, 13), (1, 12), (1, 11), (2, 11), (2, 10), (2, 9), (3, 9),
        (3, 8), (4, 8), (4, 7), (4, 6), (4, 5), (4, 4), (4, 3), (4, 2),
        (5, 2), (5, 1),
    ]
    route_a_lower: List[Point] = [
        (1, 13), (2, 13), (3, 13), (4, 13), (5, 13), (6, 13), (6, 12),
        (6, 11), (7, 11), (7, 10), (7, 9), (6, 9), (6, 8), (6, 7), (7, 7),
        (7, 6), (6, 6), (6, 5), (6, 4), (6, 3), (6, 2), (5, 2), (5, 1),
    ]
    route_b_upper = rotate_route(route_a_lower)
    route_b_lower = rotate_route(route_a_upper)

    blocked = set()
    blocked |= expand_rect([0], range(0, 5))
    blocked |= expand_rect([0], range(22, 28))
    blocked |= expand_rect([1], range(0, 3))
    blocked |= expand_rect([1], range(25, 28))
    blocked |= expand_rect([8], range(0, 3))
    blocked |= expand_rect([8], range(25, 28))
    blocked |= expand_rect([9], range(0, 5))
    blocked |= expand_rect([9], range(22, 28))
    blocked |= {(2, 4), (2, 5), (3, 4), (5, 8), (7, 4), (7, 5)}
    blocked |= {rotate_180(pos) for pos in list(blocked)}

    high_ground_a = {(2, 7), (3, 10), (5, 6), (7, 8)}
    high_ground = set(high_ground_a) | {rotate_180(pos) for pos in high_ground_a}

    deploy_a = set()
    deploy_a |= expand_rect(range(1, 5), range(1, 9))
    deploy_a |= expand_rect(range(5, 9), range(1, 8))
    deploy_a |= expand_rect(range(3, 7), range(9, 11))
    deploy_a -= blocked

    deploy_b = {rotate_180(pos) for pos in deploy_a}
    deploy_neutral = expand_rect(range(2, 8), range(11, 17))
    deploy_neutral -= blocked

    resource_points = {(4, 13), (4, 14), (5, 13), (5, 14)}
    vision_block = {(3, 12), (3, 15), (6, 12), (6, 15)}

    routes_a = [route_a_upper, route_a_lower]
    routes_b = [route_b_upper, route_b_lower]

    route_cells_a = {pos for route in routes_a for pos in route}
    route_cells_b = {pos for route in routes_b for pos in route}
    all_path_cells = route_cells_a | route_cells_b

    open_cells = {(row, col) for row in range(ROWS) for col in range(COLS)} - blocked

    for label, group in {
        "blocked": blocked,
        "high_ground": high_ground,
        "deploy_a": deploy_a,
        "deploy_b": deploy_b,
        "deploy_neutral": deploy_neutral,
        "resource": resource_points,
        "vision_block": vision_block,
        "route_a_upper": route_a_upper,
        "route_a_lower": route_a_lower,
        "route_b_upper": route_b_upper,
        "route_b_lower": route_b_lower,
        "spawns_and_cores": [spawn_a, spawn_b, core_a, core_b],
    }.items():
        assert_in_bounds(group, label)

    if blocked & all_path_cells:
        raise ValueError("blocked cells overlap a route")
    if blocked & high_ground:
        raise ValueError("blocked cells overlap high ground")

    tiles: Dict[Point, str] = {}
    for pos in open_cells:
        tiles[pos] = "EMPTY"
    for pos in blocked:
        tiles[pos] = "BLOCKED"
    for pos in high_ground:
        tiles[pos] = "HIGH_GROUND"
    for pos in all_path_cells:
        in_a = pos in route_cells_a
        in_b = pos in route_cells_b
        tiles[pos] = "PATH_SHARED" if in_a and in_b else ("PATH_A" if in_a else "PATH_B")
    tiles[spawn_a] = "SPAWN_A"
    tiles[spawn_b] = "SPAWN_B"
    tiles[core_a] = "CORE_A"
    tiles[core_b] = "CORE_B"

    tile_list = [
        {"row": row, "col": col, "type": tiles[(row, col)]}
        for row in range(ROWS)
        for col in range(COLS)
    ]

    return {
        "schemaVersion": 1,
        "name": "pvp_sunny_beach",
        "displayName": "Sunny Beach Trial",
        "mode": "PVP",
        "image": "battle_pvp.png",
        "unitVisualScale": 1.0,
        "battleViewRect": {"x": 174, "y": 126, "width": 1324, "height": 552},
        "deployViewRect": {"x": 174, "y": 126, "width": 1324, "height": 552},
        "grid": {
            "rows": ROWS,
            "cols": COLS,
            "cellSize": 48,
            "cellSizeX": 48,
            "cellSizeY": 48,
        },
        "imageCrop": {"x": 0, "y": 96, "width": 1672, "height": 604},
        "metadata": {
            "template": "sunny_beach_v1",
            "trial": True,
            "generator": "tools/generate_pvp_trial_map.py",
        },
        "points": {
            "SPAWN_A": [point(*spawn_a)],
            "SPAWN_B": [point(*spawn_b)],
            "CORE_A": [point(*core_a)],
            "CORE_B": [point(*core_b)],
            "DEPLOY_A": [point(row, col) for row, col in unique_sorted(deploy_a)],
            "DEPLOY_B": [point(row, col) for row, col in unique_sorted(deploy_b)],
            "DEPLOY_NEUTRAL": [point(row, col) for row, col in unique_sorted(deploy_neutral)],
            "RESOURCE": [point(row, col) for row, col in unique_sorted(resource_points)],
            "VISION_BLOCK": [point(row, col) for row, col in unique_sorted(vision_block)],
        },
        "routes": {
            "A": [
                route_to_json("sunny_beach_upper_to_blue", route_a_upper, spawn_a, core_a),
                route_to_json("sunny_beach_lower_to_blue", route_a_lower, spawn_a, core_a),
            ],
            "B": [
                route_to_json("sunny_beach_upper_to_red", route_b_upper, spawn_b, core_b),
                route_to_json("sunny_beach_lower_to_red", route_b_lower, spawn_b, core_b),
            ],
        },
        "tiles": tile_list,
    }


def render_preview(config: Dict[str, object], image_path: Path, preview_path: Path) -> None:
    if Image is None or ImageDraw is None:
        raise RuntimeError("Pillow is required to render preview overlays")

    crop = config["imageCrop"]
    grid = config["grid"]
    cols = int(grid["cols"])
    rows = int(grid["rows"])
    crop_box = (
        int(crop["x"]),
        int(crop["y"]),
        int(crop["x"]) + int(crop["width"]),
        int(crop["y"]) + int(crop["height"]),
    )

    if image_path.exists():
        base = Image.open(image_path).convert("RGBA").crop(crop_box)
    else:
        base = Image.new("RGBA", (crop_box[2] - crop_box[0], crop_box[3] - crop_box[1]), (26, 55, 49, 255))

    draw = ImageDraw.Draw(base, "RGBA")
    cell_w = base.width / cols
    cell_h = base.height / rows

    def cell_rect(pos: Point) -> Tuple[float, float, float, float]:
        row, col = pos
        return (
            col * cell_w,
            row * cell_h,
            (col + 1) * cell_w,
            (row + 1) * cell_h,
        )

    tile_lookup = {(tile["row"], tile["col"]): tile["type"] for tile in config["tiles"]}
    overlay_colors = {
        "BLOCKED": (46, 46, 46, 180),
        "HIGH_GROUND": (244, 208, 63, 150),
        "PATH_A": (255, 153, 102, 160),
        "PATH_B": (102, 153, 255, 160),
        "PATH_SHARED": (186, 104, 200, 160),
        "SPAWN_A": (231, 76, 60, 220),
        "SPAWN_B": (52, 152, 219, 220),
        "CORE_A": (255, 99, 71, 240),
        "CORE_B": (65, 105, 225, 240),
    }

    for pos, tile_type in tile_lookup.items():
        x0, y0, x1, y1 = cell_rect(pos)
        fill = overlay_colors.get(tile_type)
        if fill:
            draw.rectangle([x0, y0, x1, y1], fill=fill)
        draw.rectangle([x0, y0, x1, y1], outline=(0, 0, 0, 64), width=1)

    for group_name, color in {
        "DEPLOY_A": (255, 236, 179, 84),
        "DEPLOY_B": (187, 222, 251, 84),
        "DEPLOY_NEUTRAL": (200, 230, 201, 84),
        "RESOURCE": (128, 203, 196, 180),
        "VISION_BLOCK": (109, 76, 65, 180),
    }.items():
        for entry in config["points"].get(group_name, []):
            pos = (entry["row"], entry["col"])
            x0, y0, x1, y1 = cell_rect(pos)
            draw.rounded_rectangle([x0 + 4, y0 + 4, x1 - 4, y1 - 4], radius=8, outline=color, width=3)

    def draw_route(route: Dict[str, object], line_color: Tuple[int, int, int, int]) -> None:
        coords: List[Tuple[float, float]] = []
        for entry in route["path"]:
            row = entry["row"]
            col = entry["col"]
            coords.append(((col + 0.5) * cell_w, (row + 0.5) * cell_h))
        if len(coords) >= 2:
            draw.line(coords, fill=line_color, width=6, joint="curve")

    for route in config["routes"]["A"]:
        draw_route(route, (255, 120, 84, 255))
    for route in config["routes"]["B"]:
        draw_route(route, (84, 156, 255, 255))

    preview_path.parent.mkdir(parents=True, exist_ok=True)
    base.save(preview_path)


def generate_config(template_name: str) -> Dict[str, object]:
    if template_name != "sunny_beach":
        raise ValueError(f"unsupported template: {template_name}")
    return build_sunny_beach_template()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--template", default="sunny_beach", help="template name to generate")
    parser.add_argument("--output", required=True, help="path to write the generated JSON")
    parser.add_argument("--preview", help="optional preview overlay path")
    parser.add_argument(
        "--image",
        default="assets/ui/artwork/battle_pvp.png",
        help="source artwork used for the preview overlay",
    )
    args = parser.parse_args()

    config = generate_config(args.template)
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(config, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    if args.preview:
        render_preview(config, Path(args.image), Path(args.preview))


if __name__ == "__main__":
    main()
