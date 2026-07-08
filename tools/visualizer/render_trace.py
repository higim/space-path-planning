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
    ap.add_argument("--fps", type=int, default=30)
    args = ap.parse_args()

    trace = load_trace(args.trace)
    if args.hero:
        render_hero(trace, args.hero)
    if args.anim:
        render_anim(trace, args.anim, fps=args.fps)
    if not args.hero and not args.anim:
        render_hero(trace, "hero.png")


if __name__ == "__main__":
    main()