#!/usr/bin/env python3
"""
Grid map annotation tool for DffenseAndAttack.

This tool is intentionally standalone and uses only Python's standard library.
It lets designers place gameplay tags on a map image and export JSON/CSV files
that can later be consumed by the C++ game.
"""

from __future__ import annotations

import argparse
import math
import csv
import json
import sys
import tkinter as tk
from dataclasses import dataclass
from pathlib import Path
from tkinter import filedialog, messagebox, ttk
from typing import Dict, List, Optional, Tuple


APP_TITLE = "DffenseAndAttack Map Grid Editor"
SCHEMA_VERSION = 1


TILE_TYPES = [
    ("EMPTY", "空地/普通地块", "#f5f5f5"),
    ("BLOCKED", "障碍/不可走不可部署", "#4a4a4a"),
    ("PATH_A", "A 路怪物路径", "#ffb74d"),
    ("PATH_B", "B 路怪物路径", "#64b5f6"),
    ("PATH_SHARED", "公共怪物路径", "#ce93d8"),
    ("SPAWN_A", "A 路出生点", "#e53935"),
    ("SPAWN_B", "B 路出生点", "#1e88e5"),
    ("CORE_A", "A 方核心", "#ff5252"),
    ("CORE_B", "B 方核心", "#448aff"),
    ("DEPLOY_A", "A 方初始部署区", "#ffecb3"),
    ("DEPLOY_B", "B 方初始部署区", "#bbdefb"),
    ("DEPLOY_NEUTRAL", "双方/中立可部署区", "#c8e6c9"),
    ("VISION_BLOCK", "视野阻挡", "#6d4c41"),
    ("HIGH_GROUND", "高台/特殊地形", "#fff176"),
    ("RESOURCE", "资源点/特殊点位", "#80cbc4"),
]


TYPE_LABELS = {code: label for code, label, _ in TILE_TYPES}
TYPE_COLORS = {code: color for code, _, color in TILE_TYPES}
TYPE_CODES = [code for code, _, _ in TILE_TYPES]


PATH_TYPES = {"PATH_A", "PATH_B", "PATH_SHARED", "SPAWN_A", "SPAWN_B", "CORE_A", "CORE_B"}


@dataclass
class Cell:
    tile_type: str = "EMPTY"
    route_index_a: Optional[int] = None
    route_index_b: Optional[int] = None
    route_label_a: str = ""
    route_label_b: str = ""

    def to_dict(self, row: int, col: int) -> Dict[str, object]:
        data: Dict[str, object] = {
            "row": row,
            "col": col,
            "type": self.tile_type,
        }
        if self.route_index_a is not None:
            data["routeIndexA"] = self.route_index_a
        if self.route_index_b is not None:
            data["routeIndexB"] = self.route_index_b
        if self.route_label_a:
            data["routeLabelA"] = self.route_label_a
        if self.route_label_b:
            data["routeLabelB"] = self.route_label_b
        return data


class MapGridEditor(tk.Tk):
    def __init__(
        self,
        image_path: Optional[Path],
        rows: int,
        cols: int,
        output_path: Optional[Path],
        cell_size: int,
    ) -> None:
        super().__init__()

        self.title(APP_TITLE)
        self.geometry("1280x820")
        self.minsize(960, 640)

        self.image_path = image_path
        self.output_path = output_path
        self.rows_var = tk.IntVar(value=rows)
        self.cols_var = tk.IntVar(value=cols)
        self.cell_size_var = tk.IntVar(value=cell_size)
        self.map_mode = tk.StringVar(value="PVE")
        self.selected_type = tk.StringVar(value="PATH_A")
        self.paint_mode = tk.StringVar(value="type")
        self.current_route_a = tk.IntVar(value=1)
        self.current_route_b = tk.IntVar(value=1)
        self.show_labels = tk.BooleanVar(value=True)
        self.show_grid = tk.BooleanVar(value=True)
        self.auto_fit_image = tk.BooleanVar(value=True)

        self.source_background_image: Optional[tk.PhotoImage] = None
        self.background_image: Optional[tk.PhotoImage] = None
        self.image_downsample = 1
        self.background_item: Optional[int] = None
        self.grid: List[List[Cell]] = []
        self.routes_a: List[List[Tuple[int, int]]] = [[]]
        self.routes_b: List[List[Tuple[int, int]]] = [[]]
        self.cell_items: Dict[Tuple[int, int], int] = {}
        self.text_items: Dict[Tuple[int, int], int] = {}
        self.last_painted: Optional[Tuple[int, int]] = None

        self._build_ui()
        self._load_background_image()
        self._reset_grid()
        self._redraw_all()
        if self.auto_fit_image.get():
            self.after_idle(self._fit_image_to_view)

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        main = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        main.grid(row=0, column=0, sticky="nsew")

        canvas_frame = ttk.Frame(main)
        canvas_frame.rowconfigure(0, weight=1)
        canvas_frame.columnconfigure(0, weight=1)
        main.add(canvas_frame, weight=4)

        self.canvas = tk.Canvas(canvas_frame, bg="#20242a", highlightthickness=0)
        xbar = ttk.Scrollbar(canvas_frame, orient=tk.HORIZONTAL, command=self.canvas.xview)
        ybar = ttk.Scrollbar(canvas_frame, orient=tk.VERTICAL, command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=xbar.set, yscrollcommand=ybar.set)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        ybar.grid(row=0, column=1, sticky="ns")
        xbar.grid(row=1, column=0, sticky="ew")

        self.canvas.bind("<Button-1>", self._on_left_click)
        self.canvas.bind("<B1-Motion>", self._on_left_drag)
        self.canvas.bind("<ButtonRelease-1>", self._on_button_release)
        self.canvas.bind("<Button-2>", self._on_right_click)
        self.canvas.bind("<Button-3>", self._on_right_click)

        side_container = ttk.Frame(main)
        main.add(side_container, weight=1)
        side_container.rowconfigure(0, weight=1)
        side_container.columnconfigure(0, weight=1)

        side_canvas = tk.Canvas(side_container, highlightthickness=0)
        side_scroll = ttk.Scrollbar(side_container, orient=tk.VERTICAL, command=side_canvas.yview)
        side_canvas.configure(yscrollcommand=side_scroll.set)
        side_canvas.grid(row=0, column=0, sticky="nsew")
        side_scroll.grid(row=0, column=1, sticky="ns")

        side = ttk.Frame(side_canvas, padding=10)
        side_window = side_canvas.create_window((0, 0), window=side, anchor="nw")

        def update_side_scrollregion(_event: tk.Event) -> None:
            side_canvas.configure(scrollregion=side_canvas.bbox("all"))

        def update_side_width(event: tk.Event) -> None:
            side_canvas.itemconfigure(side_window, width=event.width)

        def scroll_side(event: tk.Event) -> None:
            delta = -1 if event.delta > 0 else 1
            side_canvas.yview_scroll(delta * 3, "units")

        side.bind("<Configure>", update_side_scrollregion)
        side_canvas.bind("<Configure>", update_side_width)
        side_canvas.bind_all("<MouseWheel>", scroll_side)

        ttk.Label(side, text="地图网格标注", font=("", 16, "bold")).pack(anchor="w")
        ttk.Label(side, text="左键绘制，拖拽批量绘制；右键擦除。").pack(anchor="w", pady=(4, 12))

        file_box = ttk.LabelFrame(side, text="文件")
        file_box.pack(fill="x", pady=6)
        ttk.Button(file_box, text="打开底图 PNG/GIF", command=self._choose_image).pack(fill="x", padx=8, pady=4)
        ttk.Button(file_box, text="加载 JSON", command=self._load_json_dialog).pack(fill="x", padx=8, pady=4)
        ttk.Button(file_box, text="保存 JSON", command=self._save_json_dialog).pack(fill="x", padx=8, pady=4)
        ttk.Button(file_box, text="导出 CSV", command=self._save_csv_dialog).pack(fill="x", padx=8, pady=4)

        map_box = ttk.LabelFrame(side, text="地图模式")
        map_box.pack(fill="x", pady=6)
        ttk.Radiobutton(map_box, text="PVE 单机地图（允许多个 A 出怪口）", variable=self.map_mode, value="PVE").pack(anchor="w", padx=8)
        ttk.Radiobutton(map_box, text="PVP 联机地图（要求 A/B 双方点位）", variable=self.map_mode, value="PVP").pack(anchor="w", padx=8)

        mode_box = ttk.LabelFrame(side, text="绘制/路线")
        mode_box.pack(fill="x", pady=6)
        ttk.Radiobutton(mode_box, text="普通地块标注", variable=self.paint_mode, value="type").pack(anchor="w", padx=8)
        ttk.Radiobutton(mode_box, text="记录 A 路顺序", variable=self.paint_mode, value="route_a").pack(anchor="w", padx=8)
        ttk.Radiobutton(mode_box, text="记录 B 路顺序", variable=self.paint_mode, value="route_b").pack(anchor="w", padx=8)
        self._spinbox_row(mode_box, "A路线号", self.current_route_a, 1, 99, self._sync_route_indexes)
        self._spinbox_row(mode_box, "B路线号", self.current_route_b, 1, 99, self._sync_route_indexes)
        ttk.Button(mode_box, text="新增 A 路线", command=lambda: self._add_route("A")).pack(fill="x", padx=8, pady=(6, 2))
        ttk.Button(mode_box, text="新增 B 路线", command=lambda: self._add_route("B")).pack(fill="x", padx=8, pady=2)
        ttk.Button(mode_box, text="清空当前 A 路线", command=lambda: self._clear_route("A")).pack(fill="x", padx=8, pady=(6, 2))
        ttk.Button(mode_box, text="清空当前 B 路线", command=lambda: self._clear_route("B")).pack(fill="x", padx=8, pady=2)

        grid_box = ttk.LabelFrame(side, text="网格")
        grid_box.pack(fill="x", pady=6)
        self._spinbox_row(grid_box, "行数", self.rows_var, 1, 200, self._resize_grid_from_controls)
        self._spinbox_row(grid_box, "列数", self.cols_var, 1, 200, self._resize_grid_from_controls)
        self._spinbox_row(grid_box, "格子像素", self.cell_size_var, 8, 160, self._redraw_all)
        ttk.Checkbutton(grid_box, text="显示网格线", variable=self.show_grid, command=self._redraw_all).pack(anchor="w", padx=8)
        ttk.Checkbutton(grid_box, text="显示路径序号", variable=self.show_labels, command=self._redraw_all).pack(anchor="w", padx=8)
        ttk.Checkbutton(grid_box, text="加载底图后自动适配", variable=self.auto_fit_image).pack(anchor="w", padx=8)
        ttk.Button(grid_box, text="适配左侧窗口显示全图", command=self._fit_image_to_view).pack(fill="x", padx=8, pady=(6, 2))
        ttk.Button(grid_box, text="恢复原图尺寸", command=self._use_original_image_size).pack(fill="x", padx=8, pady=(2, 6))

        type_box = ttk.LabelFrame(side, text="地块类型")
        type_box.pack(fill="both", expand=True, pady=6)
        type_canvas = tk.Canvas(type_box, height=260, highlightthickness=0)
        type_scroll = ttk.Scrollbar(type_box, orient=tk.VERTICAL, command=type_canvas.yview)
        type_inner = ttk.Frame(type_canvas)
        type_inner.bind("<Configure>", lambda _event: type_canvas.configure(scrollregion=type_canvas.bbox("all")))
        type_canvas.create_window((0, 0), window=type_inner, anchor="nw")
        type_canvas.configure(yscrollcommand=type_scroll.set)
        type_canvas.pack(side=tk.LEFT, fill="both", expand=True)
        type_scroll.pack(side=tk.RIGHT, fill="y")
        for code, label, color in TILE_TYPES:
            row = ttk.Frame(type_inner)
            row.pack(fill="x", padx=6, pady=2)
            swatch = tk.Label(row, bg=color, width=2)
            swatch.pack(side=tk.LEFT, padx=(0, 6))
            ttk.Radiobutton(row, text=f"{code} - {label}", variable=self.selected_type, value=code).pack(anchor="w")

        action_box = ttk.LabelFrame(side, text="操作")
        action_box.pack(fill="x", pady=6)
        ttk.Button(action_box, text="清空全部标注", command=self._clear_all).pack(fill="x", padx=8, pady=4)
        ttk.Button(action_box, text="校验地图", command=self._validate_and_show).pack(fill="x", padx=8, pady=4)

        self.status_var = tk.StringVar(value="就绪")
        ttk.Label(side, textvariable=self.status_var, wraplength=300).pack(fill="x", pady=(8, 0))

    def _spinbox_row(self, parent: ttk.Frame, label: str, variable: tk.IntVar, min_value: int, max_value: int, command) -> None:
        row = ttk.Frame(parent)
        row.pack(fill="x", padx=8, pady=4)
        ttk.Label(row, text=label, width=9).pack(side=tk.LEFT)
        spin = ttk.Spinbox(row, from_=min_value, to=max_value, textvariable=variable, width=8, command=command)
        spin.pack(side=tk.LEFT)
        spin.bind("<Return>", lambda _event: command())
        spin.bind("<FocusOut>", lambda _event: command())

    def _load_background_image(self) -> None:
        self.source_background_image = None
        self.background_image = None
        self.image_downsample = 1
        if not self.image_path:
            return
        try:
            self.source_background_image = tk.PhotoImage(file=str(self.image_path))
            self.background_image = self.source_background_image
            self.status_var.set(f"已加载底图：{self.image_path}")
        except tk.TclError as exc:
            messagebox.showwarning("底图加载失败", f"无法加载图片：{self.image_path}\n\n{exc}\n\n将使用空白网格。")

    def _fit_image_to_view(self) -> None:
        if not self.source_background_image:
            self._fit_grid_to_canvas()
            self._redraw_all()
            return

        self.update_idletasks()
        target_width = max(320, self.canvas.winfo_width() - 24)
        target_height = max(240, self.canvas.winfo_height() - 24)
        image_width = self.source_background_image.width()
        image_height = self.source_background_image.height()
        downsample = max(1, math.ceil(max(image_width / target_width, image_height / target_height)))

        self.image_downsample = downsample
        self.background_image = (
            self.source_background_image
            if downsample == 1
            else self.source_background_image.subsample(downsample, downsample)
        )
        self._fit_grid_to_image()
        self._redraw_all()

    def _use_original_image_size(self) -> None:
        if self.source_background_image:
            self.image_downsample = 1
            self.background_image = self.source_background_image
            self._fit_grid_to_image()
        self._redraw_all()

    def _fit_grid_to_canvas(self) -> None:
        self.update_idletasks()
        rows = self._safe_int(self.rows_var.get(), 1)
        cols = self._safe_int(self.cols_var.get(), 1)
        target_width = max(320, self.canvas.winfo_width() - 24)
        target_height = max(240, self.canvas.winfo_height() - 24)
        fitted = max(8, min(target_width // cols, target_height // rows))
        self.cell_size_var.set(fitted)

    def _fit_grid_to_image(self) -> None:
        if not self.background_image:
            self._fit_grid_to_canvas()
            return
        rows = self._safe_int(self.rows_var.get(), 1)
        cols = self._safe_int(self.cols_var.get(), 1)
        fitted = max(8, min(self.background_image.width() // cols, self.background_image.height() // rows))
        self.cell_size_var.set(fitted)

    def _reset_grid(self) -> None:
        rows = self._safe_int(self.rows_var.get(), 1)
        cols = self._safe_int(self.cols_var.get(), 1)
        self.grid = [[Cell() for _ in range(cols)] for _ in range(rows)]
        self.routes_a = [[]]
        self.routes_b = [[]]
        self.current_route_a.set(1)
        self.current_route_b.set(1)

    def _resize_grid_from_controls(self) -> None:
        old_grid = self.grid
        old_rows = len(old_grid)
        old_cols = len(old_grid[0]) if old_rows else 0
        new_rows = self._safe_int(self.rows_var.get(), 1)
        new_cols = self._safe_int(self.cols_var.get(), 1)

        new_grid = [[Cell() for _ in range(new_cols)] for _ in range(new_rows)]
        for row in range(min(old_rows, new_rows)):
            for col in range(min(old_cols, new_cols)):
                new_grid[row][col] = old_grid[row][col]

        self.grid = new_grid
        self.routes_a = self._clip_routes_to_grid(self.routes_a, new_rows, new_cols)
        self.routes_b = self._clip_routes_to_grid(self.routes_b, new_rows, new_cols)
        self._sync_route_indexes()
        self._redraw_all()

    def _redraw_all(self) -> None:
        self.canvas.delete("all")
        self.cell_items.clear()
        self.text_items.clear()

        cell_size = self._safe_int(self.cell_size_var.get(), 32)
        rows = len(self.grid)
        cols = len(self.grid[0]) if rows else 0
        width = cols * cell_size
        height = rows * cell_size

        if self.background_image:
            self.background_item = self.canvas.create_image(0, 0, image=self.background_image, anchor="nw")
            width = max(width, self.background_image.width())
            height = max(height, self.background_image.height())

        for row in range(rows):
            for col in range(cols):
                self._draw_cell(row, col)

        self.canvas.configure(scrollregion=(0, 0, width, height))
        self._update_status_summary()

    def _draw_cell(self, row: int, col: int) -> None:
        if row >= len(self.grid) or col >= len(self.grid[row]):
            return

        cell_size = self._safe_int(self.cell_size_var.get(), 32)
        x1 = col * cell_size
        y1 = row * cell_size
        x2 = x1 + cell_size
        y2 = y1 + cell_size
        cell = self.grid[row][col]
        is_empty = cell.tile_type == "EMPTY" and cell.route_index_a is None and cell.route_index_b is None
        color = "" if is_empty else TYPE_COLORS.get(cell.tile_type, "#ffffff")
        stipple = "" if is_empty else "gray50"
        outline = "#607d8b" if self.show_grid.get() else ""

        old_item = self.cell_items.pop((row, col), None)
        if old_item:
            self.canvas.delete(old_item)
        old_text = self.text_items.pop((row, col), None)
        if old_text:
            self.canvas.delete(old_text)

        item = self.canvas.create_rectangle(
            x1,
            y1,
            x2,
            y2,
            fill=color,
            outline=outline,
            stipple=stipple,
            tags=("cell", f"cell-{row}-{col}"),
        )
        self.cell_items[(row, col)] = item

        if self.show_labels.get():
            label = ""
            if cell.route_label_a:
                label = cell.route_label_a
            if cell.route_label_b:
                label = f"{label}/{cell.route_label_b}" if label else cell.route_label_b
            if cell.tile_type in {"SPAWN_A", "SPAWN_B", "CORE_A", "CORE_B"}:
                label = cell.tile_type.replace("_", "\n")
            if label:
                text_item = self.canvas.create_text(
                    (x1 + x2) / 2,
                    (y1 + y2) / 2,
                    text=label,
                    fill="#111111",
                    font=("", max(8, min(12, cell_size // 3)), "bold"),
                    tags=("cell-label",),
                )
                self.text_items[(row, col)] = text_item

    def _on_left_click(self, event: tk.Event) -> None:
        self.last_painted = None
        self._paint_from_event(event, erase=False)

    def _on_left_drag(self, event: tk.Event) -> None:
        if self.paint_mode.get() == "type":
            self._paint_from_event(event, erase=False)

    def _on_right_click(self, event: tk.Event) -> None:
        self.last_painted = None
        self._paint_from_event(event, erase=True)

    def _on_button_release(self, _event: tk.Event) -> None:
        self.last_painted = None

    def _paint_from_event(self, event: tk.Event, erase: bool) -> None:
        cell = self._event_to_cell(event)
        if cell is None:
            return
        if self.last_painted == cell:
            return
        self.last_painted = cell

        row, col = cell
        if erase:
            self.grid[row][col] = Cell()
            self.routes_a = self._remove_cell_from_routes(self.routes_a, row, col)
            self.routes_b = self._remove_cell_from_routes(self.routes_b, row, col)
            self._sync_route_indexes()
        else:
            mode = self.paint_mode.get()
            selected = self.selected_type.get()
            if mode == "route_a":
                self.grid[row][col].tile_type = "PATH_A" if selected == "EMPTY" else selected
                route = self._current_route("A")
                if (row, col) not in route:
                    route.append((row, col))
                self._sync_route_indexes()
            elif mode == "route_b":
                self.grid[row][col].tile_type = "PATH_B" if selected == "EMPTY" else selected
                route = self._current_route("B")
                if (row, col) not in route:
                    route.append((row, col))
                self._sync_route_indexes()
            else:
                self.grid[row][col].tile_type = selected
                if selected not in PATH_TYPES:
                    self.routes_a = self._remove_cell_from_routes(self.routes_a, row, col)
                    self.routes_b = self._remove_cell_from_routes(self.routes_b, row, col)
                    self._sync_route_indexes()

        self._draw_cell(row, col)
        self._update_status_summary()

    def _event_to_cell(self, event: tk.Event) -> Optional[Tuple[int, int]]:
        cell_size = self._safe_int(self.cell_size_var.get(), 32)
        x = int(self.canvas.canvasx(event.x))
        y = int(self.canvas.canvasy(event.y))
        col = x // cell_size
        row = y // cell_size
        if row < 0 or col < 0 or row >= len(self.grid) or col >= len(self.grid[row]):
            return None
        return row, col

    def _sync_route_indexes(self) -> None:
        for row in self.grid:
            for cell in row:
                cell.route_index_a = None
                cell.route_index_b = None
                cell.route_label_a = ""
                cell.route_label_b = ""
        self._ensure_route_exists("A")
        self._ensure_route_exists("B")
        for route_number, route in enumerate(self.routes_a, start=1):
            for index, (row, col) in enumerate(route):
                if row < len(self.grid) and col < len(self.grid[row]):
                    self.grid[row][col].route_index_a = index
                    self.grid[row][col].route_label_a = f"A{route_number}:{index}"
        for route_number, route in enumerate(self.routes_b, start=1):
            for index, (row, col) in enumerate(route):
                if row < len(self.grid) and col < len(self.grid[row]):
                    self.grid[row][col].route_index_b = index
                    self.grid[row][col].route_label_b = f"B{route_number}:{index}"
        self._redraw_all()

    def _route_list(self, route_name: str) -> List[List[Tuple[int, int]]]:
        return self.routes_a if route_name == "A" else self.routes_b

    def _current_route_number(self, route_name: str) -> int:
        try:
            value = self.current_route_a.get() if route_name == "A" else self.current_route_b.get()
            return max(1, int(value))
        except (TypeError, ValueError, tk.TclError):
            return 1

    def _ensure_route_exists(self, route_name: str) -> None:
        routes = self._route_list(route_name)
        target = self._current_route_number(route_name)
        while len(routes) < target:
            routes.append([])
        if not routes:
            routes.append([])

    def _current_route(self, route_name: str) -> List[Tuple[int, int]]:
        self._ensure_route_exists(route_name)
        routes = self._route_list(route_name)
        return routes[self._current_route_number(route_name) - 1]

    def _add_route(self, route_name: str) -> None:
        routes = self._route_list(route_name)
        routes.append([])
        if route_name == "A":
            self.current_route_a.set(len(routes))
        else:
            self.current_route_b.set(len(routes))
        self._sync_route_indexes()

    def _clip_routes_to_grid(self, routes: List[List[Tuple[int, int]]], rows: int, cols: int) -> List[List[Tuple[int, int]]]:
        clipped = [
            [(r, c) for r, c in route if 0 <= r < rows and 0 <= c < cols]
            for route in routes
        ]
        return clipped or [[]]

    def _remove_cell_from_routes(self, routes: List[List[Tuple[int, int]]], row: int, col: int) -> List[List[Tuple[int, int]]]:
        cleaned = [[(r, c) for r, c in route if (r, c) != (row, col)] for route in routes]
        return cleaned or [[]]

    def _choose_image(self) -> None:
        path = filedialog.askopenfilename(
            title="选择地图底图",
            filetypes=[("Images", "*.png *.gif"), ("All files", "*.*")],
        )
        if not path:
            return
        self.image_path = Path(path)
        self._load_background_image()
        if self.auto_fit_image.get():
            self._fit_image_to_view()
        self._redraw_all()

    def _load_json_dialog(self) -> None:
        path = filedialog.askopenfilename(
            title="加载地图 JSON",
            filetypes=[("JSON", "*.json"), ("All files", "*.*")],
        )
        if path:
            self._load_json(Path(path))

    def _save_json_dialog(self) -> None:
        initial = str(self.output_path) if self.output_path else "map_annotated.json"
        path = filedialog.asksaveasfilename(
            title="保存地图 JSON",
            initialfile=Path(initial).name,
            defaultextension=".json",
            filetypes=[("JSON", "*.json"), ("All files", "*.*")],
        )
        if path:
            self._save_json(Path(path))

    def _save_csv_dialog(self) -> None:
        initial = "map_tiles.csv"
        if self.output_path:
            initial = self.output_path.with_suffix(".csv").name
        path = filedialog.asksaveasfilename(
            title="导出地块 CSV",
            initialfile=initial,
            defaultextension=".csv",
            filetypes=[("CSV", "*.csv"), ("All files", "*.*")],
        )
        if path:
            self._save_csv(Path(path))

    def _load_json(self, path: Path) -> None:
        with path.open("r", encoding="utf-8") as file:
            data = json.load(file)

        grid_data = data.get("grid", {})
        rows = int(grid_data.get("rows", data.get("rows", 12)))
        cols = int(grid_data.get("cols", data.get("cols", 16)))
        cell_size = int(grid_data.get("cellSize", self.cell_size_var.get()))
        self.map_mode.set(str(data.get("mode", data.get("mapMode", "PVE"))).upper())

        self.rows_var.set(rows)
        self.cols_var.set(cols)
        self.cell_size_var.set(cell_size)
        image = data.get("image")
        if image:
            candidate = self._resolve_image_path(path, str(image))
            self.image_path = candidate
            self._load_background_image()
            if self.auto_fit_image.get():
                self._fit_image_to_view()

        self.grid = [[Cell() for _ in range(cols)] for _ in range(rows)]
        for tile in data.get("tiles", []):
            row = int(tile.get("row", -1))
            col = int(tile.get("col", -1))
            tile_type = str(tile.get("type", "EMPTY"))
            if 0 <= row < rows and 0 <= col < cols and tile_type in TYPE_CODES:
                self.grid[row][col].tile_type = tile_type

        self.routes_a = self._read_routes(data, "A")
        self.routes_b = self._read_routes(data, "B")
        self.current_route_a.set(1)
        self.current_route_b.set(1)
        self._sync_route_indexes()
        self.output_path = path
        self._redraw_all()
        self.status_var.set(f"已加载：{path}")

    def _read_routes(self, data: Dict[str, object], route_name: str) -> List[List[Tuple[int, int]]]:
        route_data = data.get("routes", {})
        if not isinstance(route_data, dict):
            return [[]]
        route_value = route_data.get(route_name, [])
        routes: List[List[Tuple[int, int]]] = []
        if isinstance(route_value, list):
            if route_value and all(isinstance(item, dict) and "path" in item for item in route_value):
                for route in route_value:
                    routes.append(self._read_point_list(route.get("path", [])))
            else:
                legacy_route = self._read_point_list(route_value)
                if legacy_route:
                    routes.append(legacy_route)
        return routes or [[]]

    def _read_point_list(self, points: object) -> List[Tuple[int, int]]:
        result = []
        if isinstance(points, list):
            for point in points:
                if isinstance(point, dict):
                    result.append((int(point.get("row", 0)), int(point.get("col", 0))))
                elif isinstance(point, list) and len(point) >= 2:
                    result.append((int(point[0]), int(point[1])))
        return result

    def _save_json(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        warnings = self._validate()
        data = self._build_export_data(path)
        with path.open("w", encoding="utf-8") as file:
            json.dump(data, file, ensure_ascii=False, indent=2)
            file.write("\n")
        self.output_path = path
        self.status_var.set(f"已保存 JSON：{path}")
        if warnings:
            messagebox.showwarning("地图已保存，但存在校验提醒", "\n".join(warnings))

    def _save_csv(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="") as file:
            writer = csv.writer(file)
            writer.writerow(["row", "col", "type", "routeIndexA", "routeLabelA", "routeIndexB", "routeLabelB"])
            for row_index, row in enumerate(self.grid):
                for col_index, cell in enumerate(row):
                    writer.writerow([
                        row_index,
                        col_index,
                        cell.tile_type,
                        "" if cell.route_index_a is None else cell.route_index_a,
                        cell.route_label_a,
                        "" if cell.route_index_b is None else cell.route_index_b,
                        cell.route_label_b,
                    ])
        self.status_var.set(f"已导出 CSV：{path}")

    def _build_export_data(self, output_path: Path) -> Dict[str, object]:
        image_value = None
        if self.image_path:
            try:
                image_value = str(self.image_path.resolve().relative_to(output_path.parent.resolve()))
            except ValueError:
                image_value = str(self.image_path.resolve())

        tiles = []
        for row_index, row in enumerate(self.grid):
            for col_index, cell in enumerate(row):
                if cell.tile_type != "EMPTY" or cell.route_index_a is not None or cell.route_index_b is not None:
                    tiles.append(cell.to_dict(row_index, col_index))

        return {
            "schemaVersion": SCHEMA_VERSION,
            "name": output_path.stem,
            "mode": self.map_mode.get(),
            "image": image_value,
            "grid": {
                "rows": len(self.grid),
                "cols": len(self.grid[0]) if self.grid else 0,
                "cellSize": self._safe_int(self.cell_size_var.get(), 32),
            },
            "legend": {code: TYPE_LABELS[code] for code in TYPE_CODES},
            "routes": {
                "A": self._format_routes_for_export("A", self.routes_a),
                "B": self._format_routes_for_export("B", self.routes_b),
            },
            "points": self._collect_points(),
            "tiles": tiles,
        }

    def _format_routes_for_export(self, route_name: str, routes: List[List[Tuple[int, int]]]) -> List[Dict[str, object]]:
        exported = []
        for index, route in enumerate(routes, start=1):
            if not route:
                continue
            exported.append({
                "id": f"{route_name}{index}",
                "spawn": {"row": route[0][0], "col": route[0][1]},
                "core": {"row": route[-1][0], "col": route[-1][1]},
                "path": [{"row": row, "col": col} for row, col in route],
            })
        return exported

    def _collect_points(self) -> Dict[str, List[Dict[str, int]]]:
        point_types = ["SPAWN_A", "SPAWN_B", "CORE_A", "CORE_B", "RESOURCE"]
        points = {tile_type: [] for tile_type in point_types}
        for row_index, row in enumerate(self.grid):
            for col_index, cell in enumerate(row):
                if cell.tile_type in points:
                    points[cell.tile_type].append({"row": row_index, "col": col_index})
        return points

    def _clear_route(self, route_name: str) -> None:
        self._ensure_route_exists(route_name)
        if route_name == "A":
            self.routes_a[self._current_route_number("A") - 1] = []
        else:
            self.routes_b[self._current_route_number("B") - 1] = []
        self._sync_route_indexes()
        self._redraw_all()

    def _clear_all(self) -> None:
        if not messagebox.askyesno("确认清空", "确定要清空所有地块标注和路线顺序吗？"):
            return
        self._reset_grid()
        self._redraw_all()

    def _validate_and_show(self) -> None:
        warnings = self._validate()
        if warnings:
            messagebox.showwarning("地图校验提醒", "\n".join(warnings))
        else:
            messagebox.showinfo("地图校验", "基础校验通过。")

    def _validate(self) -> List[str]:
        warnings = []
        mode = self.map_mode.get().upper()
        points = self._collect_points()
        if not points["SPAWN_A"]:
            warnings.append("缺少 SPAWN_A。")
        if not points["CORE_A"]:
            warnings.append("缺少 CORE_A。")
        if mode == "PVP":
            if not points["SPAWN_B"]:
                warnings.append("PVP 地图缺少 SPAWN_B。")
            if not points["CORE_B"]:
                warnings.append("PVP 地图缺少 CORE_B。")
            if not self._non_empty_routes(self.routes_b):
                warnings.append("PVP 地图 B 路顺序为空。")
        if not self._non_empty_routes(self.routes_a):
            warnings.append("A 路顺序为空。")

        for route_name, routes, core_key in [
            ("A", self.routes_a, "CORE_A"),
            ("B", self.routes_b, "CORE_B"),
        ]:
            if route_name == "B" and mode != "PVP" and not self._non_empty_routes(routes):
                continue
            core_positions = {(point["row"], point["col"]) for point in points[core_key]}
            spawn_positions = {(point["row"], point["col"]) for point in points[f"SPAWN_{route_name}"]}
            for index, route in enumerate(routes, start=1):
                if not route:
                    continue
                if spawn_positions and route[0] not in spawn_positions:
                    warnings.append(f"{route_name}{index} 第一个点不是 {route_name} 出生点。")
                if core_positions and route[-1] not in core_positions:
                    warnings.append(f"{route_name}{index} 最后一个点不是 {route_name} 核心。")
                for prev, current in zip(route, route[1:]):
                    if abs(prev[0] - current[0]) + abs(prev[1] - current[1]) != 1:
                        warnings.append(f"{route_name}{index} 存在不相邻路径点：{prev} -> {current}。")
        return warnings

    @staticmethod
    def _non_empty_routes(routes: List[List[Tuple[int, int]]]) -> List[List[Tuple[int, int]]]:
        return [route for route in routes if route]

    @staticmethod
    def _resolve_image_path(json_path: Path, image: str) -> Path:
        raw = Path(image)
        candidates = []
        if raw.is_absolute():
            candidates.append(raw)
        else:
            candidates.append(json_path.parent / raw)
            candidates.append(Path.cwd() / raw)
            candidates.append(json_path.parent / raw.name)
        for candidate in candidates:
            if candidate.exists():
                return candidate
        return candidates[0] if candidates else raw

    def _update_status_summary(self) -> None:
        counts: Dict[str, int] = {}
        for row in self.grid:
            for cell in row:
                if cell.tile_type != "EMPTY":
                    counts[cell.tile_type] = counts.get(cell.tile_type, 0) + 1
        summary = ", ".join(f"{key}:{value}" for key, value in sorted(counts.items()))
        routes_a_count = len(self._non_empty_routes(self.routes_a))
        routes_b_count = len(self._non_empty_routes(self.routes_b))
        route_a_points = sum(len(route) for route in self.routes_a)
        route_b_points = sum(len(route) for route in self.routes_b)
        self.status_var.set(
            f"{self.map_mode.get()} | 网格 {len(self.grid)}x{len(self.grid[0]) if self.grid else 0} | "
            f"A路 {routes_a_count}条/{route_a_points}点 | B路 {routes_b_count}条/{route_b_points}点"
            + (f" | {summary}" if summary else "")
        )

    @staticmethod
    def _safe_int(value: object, fallback: int) -> int:
        try:
            return max(1, int(value))
        except (TypeError, ValueError, tk.TclError):
            return fallback


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Annotate a map image into gameplay grid data.")
    parser.add_argument("--image", type=Path, help="Map background image. PNG/GIF is supported by the stdlib Tk build.")
    parser.add_argument("--rows", type=int, default=12, help="Grid rows.")
    parser.add_argument("--cols", type=int, default=16, help="Grid columns.")
    parser.add_argument("--cell-size", type=int, default=48, help="Cell size in pixels.")
    parser.add_argument("--output", type=Path, help="Default JSON output path.")
    parser.add_argument("--load", type=Path, help="Load an existing exported JSON file.")
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = parse_args(argv)
    editor = MapGridEditor(args.image, args.rows, args.cols, args.output, args.cell_size)
    if args.load:
        editor._load_json(args.load)
    editor.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
