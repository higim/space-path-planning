"""
Mars A* Trace Visualizer
========================
Renders a v1 search trace (see TRACE_FORMAT.md) three ways:
  - hero image  : final path over the fully-explored grid (the article's key art)
  - animation   : the A* frontier wave expanding, then the path drawn

Mars palette: dark regolith background, dust-orange terrain heat, a pale
frontier glow at the leading edge of the search, and a bright path.

Usage:
  python render_trace.py sample_astar.json --hero hero.png
  python render_trace.py sample_astar.json --anim search.gif
  python render_trace.py sample_astar.json --hero hero.png --anim search.gif
"""
import json
import argparse
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap
from matplotlib.patches import Rectangle
import matplotlib.animation as animation

OBSTACLE_THRESHOLD = 1_000_000

# ---- Mars palette -------------------------------------------------------
MARS = {
    "space":     "#0d0a08",   # deep background (near-black, warm)
    "regolith":  "#2a2016",   # cheap terrain (dark martian soil)
    "dust_lo":   "#5c3a1e",   # low rocky cost
    "dust_hi":   "#b5651d",   # high rocky cost (dust orange)
    "obstacle":  "#080604",   # impassable rock (near-black)
    "explored":  "#3d3320",   # cells A* has closed
    "frontier":  "#ffd27f",   # open-set leading edge (pale glow)
    "frontier2": "#ff9d4d",   # frontier core
    "path":      "#7fe3ff",   # final path (cool cyan pops against orange)
    "start":     "#39d98a",   # green
    "goal":      "#ff5c38",   # mars red-orange
    "grid":      "#1a140d",   # cell gridlines
    "text":      "#e8d5b0",
}

# terrain colormap: regolith -> dust orange
TERRAIN = LinearSegmentedColormap.from_list(
    "mars_terrain", [MARS["regolith"], MARS["dust_lo"], MARS["dust_hi"]]
)


def load_trace(path):
    with open(path) as fp:
        return json.load(fp)


def terrain_rgb(costs, H, W):
    """Base terrain image (obstacles dark, terrain by cost)."""
    arr = np.array(costs, dtype=float)
    obstacle_mask = arr >= OBSTACLE_THRESHOLD

    # Normalize non-obstacle costs to [0,1] for the colormap
    finite = arr.copy()
    finite[obstacle_mask] = np.nan
    vmax = np.nanmax(finite) if np.isfinite(np.nanmax(finite)) else 1.0
    vmin = 1.0
    norm = (finite - vmin) / max(1e-9, (vmax - vmin))
    norm = np.clip(np.nan_to_num(norm, nan=0.0), 0, 1)

    rgb = TERRAIN(norm)[:, :, :3]  # drop alpha

    # paint obstacles
    obs = np.array([int(MARS["obstacle"][i:i+2], 16)/255 for i in (1,3,5)])
    rgb[obstacle_mask] = obs
    return rgb


def hex_rgb(h):
    return np.array([int(h[i:i+2],16)/255 for i in (1,3,5)])


def render_hero(trace, out_path):
    """Final path over the fully explored grid — the article hero image."""
    meta = trace["meta"]; H, W = meta["height"], meta["width"]
    costs = trace["grid"]["costs"]
    base = terrain_rgb(costs, H, W)

    # Dim explored cells slightly brighter than untouched terrain
    explored = hex_rgb(MARS["explored"])
    img = base.copy()
    for s in trace["steps"]:
        e = s["expanded"]
        # blend explored tint over terrain (keeps cost hue but marks visited)
        img[e["row"], e["col"]] = 0.55*img[e["row"], e["col"]] + 0.45*explored

    fig, ax = _make_ax(H, W)
    ax.imshow(img, origin="upper", interpolation="nearest",
              extent=[0, W, H, 0], zorder=1)

    # Path as a bright polyline through cell centers
    path = trace["path"]
    if path:
        xs = [p["col"]+0.5 for p in path]
        ys = [p["row"]+0.5 for p in path]
        ax.plot(xs, ys, color=MARS["path"], lw=2.6, zorder=4,
                solid_capstyle="round", solid_joinstyle="round")
        ax.plot(xs, ys, color="white", lw=0.8, alpha=0.5, zorder=5,
                solid_capstyle="round")

    _draw_endpoints(ax, trace)
    _annotate(ax, trace, W, H)
    fig.savefig(out_path, dpi=150, bbox_inches="tight",
                facecolor=MARS["space"], pad_inches=0.15)
    plt.close(fig)
    print(f"hero image -> {out_path}")


def render_costmap(trace, out_path, annotate_costs=True):
    """The raw traversability grid: each cell tinted and labelled with its cost.

    No search, no path — just the stage A* performs on. Flat regolith is cheap
    (dark, low number), rocky terrain is expensive (orange, high number), and
    obstacles are impassable black cells with no number at all.
    """
    meta = trace["meta"]; H, W = meta["height"], meta["width"]
    costs = trace["grid"]["costs"]
    base = terrain_rgb(costs, H, W)

    fig, ax = _make_ax(H, W)
    ax.imshow(base, origin="upper", interpolation="nearest",
              extent=[0, W, H, 0], zorder=1)

    if annotate_costs:
        arr = np.array(costs, dtype=float)
        for r in range(H):
            for c in range(W):
                v = arr[r, c]
                if v >= OBSTACLE_THRESHOLD:
                    continue
                # dark text on cheap (light-ish) cells, light text on hot cells
                shade = MARS["space"] if v >= 4 else MARS["text"]
                ax.text(c + 0.5, r + 0.5, f"{int(v)}", ha="center", va="center",
                        color=shade, fontsize=6.5, family="monospace", zorder=3)

    _draw_endpoints(ax, trace)
    ax.text(0.02, -0.04,
            "traversability grid  ·  1 = flat regolith  …  5 = rocky  ·  black = impassable",
            transform=ax.transAxes, color=MARS["text"], fontsize=9,
            family="monospace", ha="left", va="top")
    fig.savefig(out_path, dpi=150, bbox_inches="tight",
                facecolor=MARS["space"], pad_inches=0.15)
    plt.close(fig)
    print(f"cost map -> {out_path}")


def render_anim(trace, out_path, fps=30, tail=45):
    """Animate the frontier wave, then draw the path.

    To keep the GIF light, we sample expansions so the whole search plays in a
    few seconds regardless of grid size. The frontier glow uses the per-step
    frontier snapshot from the trace.
    """
    meta = trace["meta"]; H, W = meta["height"], meta["width"]
    costs = trace["grid"]["costs"]
    base = terrain_rgb(costs, H, W)
    explored_tint = hex_rgb(MARS["explored"])
    front_lo = hex_rgb(MARS["frontier"])
    front_hi = hex_rgb(MARS["frontier2"])

    steps = trace["steps"]
    n = len(steps)

    # frame schedule: sample search steps to ~ (fps * seconds) frames
    search_frames = min(n, 150)
    idxs = np.linspace(0, n-1, search_frames).astype(int)
    path = trace["path"]
    path_frames = len(path) if path else 0
    total_frames = len(idxs) + path_frames + 20  # +hold at end

    fig, ax = _make_ax(H, W)
    im = ax.imshow(base.copy(), origin="upper", interpolation="nearest",
                   extent=[0, W, H, 0], zorder=1)
    path_line, = ax.plot([], [], color=MARS["path"], lw=2.6, zorder=4,
                         solid_capstyle="round", solid_joinstyle="round")
    _draw_endpoints(ax, trace)
    title = _annotate(ax, trace, W, H, live=True)

    # persistent explored accumulation buffer
    canvas = base.copy()
    accumulated = 0

    def frame(fi):
        nonlocal canvas, accumulated
        if fi < len(idxs):
            # grow exploration up to idxs[fi]
            upto = idxs[fi]
            while accumulated <= upto:
                e = steps[accumulated]["expanded"]
                canvas[e["row"], e["col"]] = (0.55*canvas[e["row"], e["col"]]
                                              + 0.45*explored_tint)
                accumulated += 1
            img = canvas.copy()
            # draw current frontier as glow
            fr = steps[upto]["frontier"]
            for cell in fr:
                img[cell["row"], cell["col"]] = front_lo
            # highlight the just-expanded cell hot
            e = steps[upto]["expanded"]
            img[e["row"], e["col"]] = front_hi
            im.set_data(img)
            title.set_text(f"A*  ·  expanded {upto+1}/{n}")
            path_line.set_data([], [])
        else:
            # exploration done; draw the path progressively
            im.set_data(canvas)
            k = fi - len(idxs)
            k = min(k, path_frames)
            if path:
                xs = [p["col"]+0.5 for p in path[:k]]
                ys = [p["row"]+0.5 for p in path[:k]]
                path_line.set_data(xs, ys)
            title.set_text(f"A*  ·  path found  ·  {len(path)} cells")
        return [im, path_line, title]

    anim = animation.FuncAnimation(fig, frame, frames=total_frames,
                                   interval=1000/fps, blit=False)
    anim.save(out_path, writer=animation.PillowWriter(fps=fps),
              savefig_kwargs={"facecolor": MARS["space"]})
    plt.close(fig)
    print(f"animation -> {out_path}")


# ---- Part 2: replanning drive ------------------------------------------

ALGO_DISPLAY = {"astar": "A*", "dstar": "D*", "dlite": "D* Lite"}


def algo_name(meta):
    a = meta.get("algorithm", "?")
    return ALGO_DISPLAY.get(a, a)


def phases_of(trace):
    """Normalize a trace into a list of phases: the initial search first, then
    each replan. Every phase has origin/changes/steps/path."""
    g = trace["grid"]
    phases = [{"origin": g["start"], "changes": [], "steps": trace["steps"],
               "path": trace["path"]}]
    phases.extend(trace.get("replans", []))
    return phases


def _idx_of(path, cell):
    for i, p in enumerate(path):
        if p["row"] == cell["row"] and p["col"] == cell["col"]:
            return i
    return len(path) - 1 if path else 0


def traveled_route(phases, i, k):
    """The cells the rover has actually driven by the time it's at phase i,
    having advanced k steps into phase i's path (k=None => still at the origin).
    Each phase contributes its path up to the next phase's origin; consecutive
    segments share that origin cell, so we drop the duplicate."""
    route = []

    def extend(seg):
        nonlocal route
        route = list(seg) if not route else route + seg[1:]

    for j in range(i):
        stop = phases[j + 1]["origin"]
        extend(phases[j]["path"][:_idx_of(phases[j]["path"], stop) + 1])
    if k is not None and k >= 0 and phases[i]["path"]:
        extend(phases[i]["path"][:k + 1])
    elif not route and phases[i]["path"]:
        route = [phases[i]["path"][0]]
    return route


def render_drive_anim(trace, out_path, fps=24):
    """Animate a full sensor-limited drive: initial search, then for each replan
    the rover advances, obstacles appear, and the planner reacts. A* re-searches
    a full cone every time; D* Lite mostly flickers locally. Same code, both."""
    meta = trace["meta"]; H, W = meta["height"], meta["width"]
    name = algo_name(meta)
    phases = phases_of(trace)
    goal = trace["grid"]["goal"]

    costs = np.array(trace["grid"]["costs"], dtype=float)
    front_lo   = hex_rgb(MARS["frontier"])    # this phase's expanded cells (bright)
    front_hi   = hex_rgb(MARS["frontier2"])   # the cell just expanded (hot)

    # Build an explicit frame plan so the (sequential) animation stays simple.
    # Each phase contributes a few search frames then a few drive frames.
    plan = []
    for i, ph in enumerate(phases):
        n = len(ph["steps"])
        sf = 1 if n == 0 else int(np.clip(n // 3, 3, 22))
        for idx in (np.linspace(0, n - 1, sf).astype(int) if n else [-1]):
            plan.append(("search", i, int(idx)))
        stop = phases[i + 1]["origin"] if i + 1 < len(phases) else goal
        path = ph["path"]
        stop_idx = _idx_of(path, stop) if path else 0
        df = int(np.clip(stop_idx, 1, 12))
        for k in (np.linspace(0, stop_idx, df).astype(int) if stop_idx else [0]):
            plan.append(("drive", i, int(k)))
    # hold on the final frame with the rover parked ON the goal (k = goal index in
    # the last phase's path), not back at the last replan origin (k = 0).
    last = len(phases) - 1
    last_goal_k = _idx_of(phases[last]["path"], goal) if phases[last]["path"] else 0
    plan += [("hold", last, last_goal_k)] * 16

    fig, ax = _make_ax(H, W)
    base = terrain_rgb(costs.tolist(), H, W)
    im = ax.imshow(base.copy(), origin="upper", interpolation="nearest",
                   extent=[0, W, H, 0], zorder=1)
    traveled_line, = ax.plot([], [], color=MARS["path"], lw=2.4, alpha=0.65, zorder=4,
                             solid_capstyle="round")
    planned_line,  = ax.plot([], [], color=MARS["path"], lw=1.6, alpha=0.9, zorder=5,
                             ls=(0, (2, 2)))
    robot = ax.add_patch(Rectangle((0, 0), 0.9, 0.9, color="#ffffff", zorder=8))
    _draw_endpoints(ax, trace)
    label = ax.text(0.02, -0.045, "", transform=ax.transAxes, color=MARS["text"],
                    fontsize=9, family="monospace", ha="left", va="top")
    tally = ax.text(0.98, -0.045, "", transform=ax.transAxes, color=MARS["text"],
                    fontsize=9, family="monospace", ha="right", va="top")

    # incremental, sequential state
    st = {"phase": -1, "base": base.copy(), "active": np.zeros((H, W), bool),
          "phase_expanded": 0}

    def enter_phase(i):
        ph = phases[i]
        st["active"][:] = False
        for ch in ph["changes"]:
            costs[ch["row"], ch["col"]] = ch["cost"]
        st["base"] = terrain_rgb(costs.tolist(), H, W)
        st["phase"] = i
        st["phase_expanded"] = 0

    def frame(fi):
        kind, i, k = plan[fi]
        if i != st["phase"]:
            enter_phase(i)
        ph = phases[i]
        img = st["base"].copy()

        if kind == "search":
            steps = ph["steps"]
            if k >= 0 and steps:
                for j in range(k + 1):
                    e = steps[j]["expanded"]
                    st["active"][e["row"], e["col"]] = True
                st["phase_expanded"] = k + 1
            img[st["active"]] = 0.4 * img[st["active"]] + 0.6 * front_lo
            if k >= 0 and steps:
                e = steps[k]["expanded"]
                img[e["row"], e["col"]] = front_hi
            planned = ph["path"]
            o = ph["origin"]
            tv = traveled_route(phases, i, None)
        else:  # drive / hold: the search flash has passed; just the rover rolling on
            path = ph["path"]
            o = path[min(k, len(path) - 1)] if path else ph["origin"]
            planned = path[k:] if kind == "drive" else path[-1:]
            tv = traveled_route(phases, i, k)

        im.set_data(np.clip(img, 0, 1))
        robot.set_xy((o["col"] + 0.05, o["row"] + 0.05))
        traveled_line.set_data([p["col"] + 0.5 for p in tv], [p["row"] + 0.5 for p in tv])
        planned_line.set_data([p["col"] + 0.5 for p in planned],
                              [p["row"] + 0.5 for p in planned])

        done = sum(len(p["steps"]) for p in phases[:i])
        running = done + (st["phase_expanded"] if kind == "search" else len(ph["steps"]))
        tag = "initial search" if i == 0 else f"replan {i}"
        label.set_text(f"{name}  ·  {tag}")
        tally.set_text(f"cells expanded so far: {running}")
        return [im, traveled_line, planned_line, robot, label, tally]

    anim = animation.FuncAnimation(fig, frame, frames=len(plan),
                                   interval=1000 / fps, blit=False)
    anim.save(out_path, writer=animation.PillowWriter(fps=fps),
              savefig_kwargs={"facecolor": MARS["space"]})
    plt.close(fig)
    total = sum(len(p["steps"]) for p in phases)
    print(f"drive animation -> {out_path}  ({name}: {len(phases)-1} replans, "
          f"{total} expansions)")


def render_drive_hero(trace, out_path):
    """The rover's whole journey: driven trail + final planned path over the map
    as finally known (obstacles and all). Blocky right-angle steps on purpose."""
    meta = trace["meta"]; H, W = meta["height"], meta["width"]
    phases = phases_of(trace)
    costs = np.array(trace["grid"]["costs"], dtype=float)
    for ph in phases:
        for ch in ph["changes"]:
            costs[ch["row"], ch["col"]] = ch["cost"]

    # stitch the actually-travelled route: each phase's path up to the next origin
    route = []
    for i, ph in enumerate(phases):
        stop = phases[i + 1]["origin"] if i + 1 < len(phases) else trace["grid"]["goal"]
        end = _idx_of(ph["path"], stop)
        seg = ph["path"][:end + 1] if ph["path"] else []
        route += seg if not route else seg[1:]

    base = terrain_rgb(costs.tolist(), H, W)
    fig, ax = _make_ax(H, W)
    ax.imshow(base, origin="upper", interpolation="nearest",
              extent=[0, W, H, 0], zorder=1)
    if route:
        xs = [p["col"] + 0.5 for p in route]; ys = [p["row"] + 0.5 for p in route]
        ax.plot(xs, ys, color=MARS["path"], lw=2.6, zorder=4,
                solid_capstyle="round", solid_joinstyle="round")
        ax.plot(xs, ys, color="white", lw=0.7, alpha=0.5, zorder=5)
    _draw_endpoints(ax, trace)
    ax.text(0.02, -0.045, f"{algo_name(meta)}  ·  the rover's actual route, "
            f"stair-stepped by the 4-direction grid", transform=ax.transAxes,
            color=MARS["text"], fontsize=9, family="monospace", ha="left", va="top")
    fig.savefig(out_path, dpi=150, bbox_inches="tight",
                facecolor=MARS["space"], pad_inches=0.15)
    plt.close(fig)
    print(f"drive hero -> {out_path}")


def render_scaling(scaling_path, out_path):
    """Line chart: total planning work vs map size, A* (replan from scratch) vs
    D* Lite (incremental). The gap widens with the map -- the article's payoff."""
    with open(scaling_path) as fp:
        pts = json.load(fp)["points"]
    cells  = [p["cells"] for p in pts]
    astar  = [p["astar"] for p in pts]
    dlite  = [p["dlite"] for p in pts]
    labels = [f'{p["w"]}x{p["h"]}' for p in pts]

    fig, ax = plt.subplots(figsize=(7, 4.3))
    fig.patch.set_facecolor(MARS["space"]); ax.set_facecolor(MARS["space"])
    ax.plot(cells, astar, "-o", color=MARS["goal"], lw=2.2, ms=6, label="A*  (replan from scratch)")
    ax.plot(cells, dlite, "-o", color=MARS["path"], lw=2.2, ms=6, label="D* Lite  (incremental)")
    for x, ya, yd in zip(cells, astar, dlite):
        ax.annotate(f"{ya}", (x, ya), color=MARS["goal"], fontsize=8,
                    family="monospace", xytext=(0, 7), textcoords="offset points", ha="center")
        ax.annotate(f"{yd}", (x, yd), color=MARS["path"], fontsize=8,
                    family="monospace", xytext=(0, -14), textcoords="offset points", ha="center")
    ax.set_xticks(cells); ax.set_xticklabels(labels)
    ax.set_xlabel("map size", color=MARS["text"], family="monospace", fontsize=9)
    ax.set_ylabel("total cells expanded over the drive", color=MARS["text"],
                  family="monospace", fontsize=9)
    for spine in ax.spines.values(): spine.set_color(MARS["grid"])
    ax.tick_params(colors=MARS["text"], labelsize=8)
    for lbl in ax.get_xticklabels() + ax.get_yticklabels(): lbl.set_family("monospace")
    ax.grid(True, color=MARS["grid"], lw=0.6, alpha=0.6)
    leg = ax.legend(facecolor=MARS["space"], edgecolor=MARS["grid"], fontsize=9,
                    labelcolor=MARS["text"])
    for t in leg.get_texts(): t.set_family("monospace")
    fig.savefig(out_path, dpi=150, bbox_inches="tight",
                facecolor=MARS["space"], pad_inches=0.2)
    plt.close(fig)
    print(f"scaling chart -> {out_path}")


def render_counter_race(astar_trace, dlite_trace, out_path):
    """The two running counters on one axis: as the rover drives across the map
    (x), how much planning work has piled up (y). A* jumps hard at every replan
    and pulls away; D* Lite climbs gently. Makes the count contrast unmissable."""
    def series(trace):
        phases = phases_of(trace)
        xs, ys, cum = [], [], 0
        for j, ph in enumerate(phases):
            cum += len(ph["steps"])
            xs.append(max(0, len(traveled_route(phases, j, None)) - 1))
            ys.append(cum)
        last = len(phases) - 1
        end_k = len(phases[last]["path"]) - 1 if phases[last]["path"] else None
        xs.append(max(0, len(traveled_route(phases, last, end_k)) - 1))
        ys.append(cum)
        return xs, ys

    ax_x, ax_y = series(astar_trace)
    dl_x, dl_y = series(dlite_trace)

    fig, ax = plt.subplots(figsize=(7, 4.3))
    fig.patch.set_facecolor(MARS["space"]); ax.set_facecolor(MARS["space"])
    ax.step(ax_x, ax_y, where="post", color=MARS["goal"], lw=2.2,
            label=f"A*  (replan from scratch)  → {ax_y[-1]}")
    ax.step(dl_x, dl_y, where="post", color=MARS["path"], lw=2.2,
            label=f"D* Lite  (incremental)  → {dl_y[-1]}")
    ax.set_xlabel("cells driven across the map", color=MARS["text"],
                  family="monospace", fontsize=9)
    ax.set_ylabel("cumulative cells expanded (planning work)", color=MARS["text"],
                  family="monospace", fontsize=9)
    for spine in ax.spines.values(): spine.set_color(MARS["grid"])
    ax.tick_params(colors=MARS["text"], labelsize=8)
    for lbl in ax.get_xticklabels() + ax.get_yticklabels(): lbl.set_family("monospace")
    ax.grid(True, color=MARS["grid"], lw=0.6, alpha=0.6)
    leg = ax.legend(facecolor=MARS["space"], edgecolor=MARS["grid"], fontsize=9,
                    labelcolor=MARS["text"], loc="upper left")
    for t in leg.get_texts(): t.set_family("monospace")
    fig.savefig(out_path, dpi=150, bbox_inches="tight",
                facecolor=MARS["space"], pad_inches=0.2)
    plt.close(fig)
    print(f"counter race -> {out_path}  (A* {ax_y[-1]} vs D* Lite {dl_y[-1]})")


# ---- shared drawing helpers --------------------------------------------

def _make_ax(H, W):
    cell_px = 26
    fig_w = max(4, W*cell_px/100)
    fig_h = max(3, H*cell_px/100)
    fig, ax = plt.subplots(figsize=(fig_w, fig_h))
    fig.patch.set_facecolor(MARS["space"])
    ax.set_facecolor(MARS["space"])
    ax.set_xlim(0, W); ax.set_ylim(H, 0)
    ax.set_xticks([]); ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)
    # faint grid lines
    for x in range(W+1):
        ax.axvline(x, color=MARS["grid"], lw=0.4, zorder=2)
    for y in range(H+1):
        ax.axhline(y, color=MARS["grid"], lw=0.4, zorder=2)
    ax.set_aspect("equal")
    return fig, ax


def _draw_endpoints(ax, trace):
    s = trace["grid"]["start"]; g = trace["grid"]["goal"]
    if s:
        ax.add_patch(Rectangle((s["col"]+0.12, s["row"]+0.12), 0.76, 0.76,
                     color=MARS["start"], zorder=6))
    if g:
        ax.add_patch(Rectangle((g["col"]+0.12, g["row"]+0.12), 0.76, 0.76,
                     color=MARS["goal"], zorder=6))


def _annotate(ax, trace, W, H, live=False):
    st = trace["stats"]
    label = (f"A* · Manhattan · 4-connected      "
             f"expanded {st['expanded_count']} · path {st['path_length']}")
    t = ax.text(0.02, -0.04, label, transform=ax.transAxes,
                color=MARS["text"], fontsize=9, family="monospace",
                ha="left", va="top")
    if live:
        return ax.text(0.98, -0.04, "", transform=ax.transAxes,
                       color=MARS["text"], fontsize=9, family="monospace",
                       ha="right", va="top")
    return t


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--hero")
    ap.add_argument("--anim")
    ap.add_argument("--costmap")
    ap.add_argument("--drive-anim", help="multi-phase replanning drive animation (Part 2)")
    ap.add_argument("--drive-hero", help="final stitched route over the fully-known map")
    ap.add_argument("--scaling", help="scaling chart from a scaling.json file")
    ap.add_argument("--counter-race", help="second (D* Lite) trace to race against the positional (A*) trace")
    ap.add_argument("--counter-out", help="output png for --counter-race")
    ap.add_argument("--fps", type=int, default=30)
    args = ap.parse_args()

    # --scaling reads its own file (the positional trace arg is ignored for it).
    if args.scaling:
        render_scaling(args.trace, args.scaling)
        return
    if args.counter_race:
        render_counter_race(load_trace(args.trace), load_trace(args.counter_race),
                            args.counter_out or "counter_race.png")
        return

    trace = load_trace(args.trace)
    if args.hero:
        render_hero(trace, args.hero)
    if args.costmap:
        render_costmap(trace, args.costmap)
    if args.anim:
        render_anim(trace, args.anim, fps=args.fps)
    if args.drive_anim:
        render_drive_anim(trace, args.drive_anim, fps=args.fps)
    if args.drive_hero:
        render_drive_hero(trace, args.drive_hero)
    if not any([args.hero, args.anim, args.costmap, args.drive_anim, args.drive_hero]):
        render_hero(trace, "hero.png")


if __name__ == "__main__":
    main()