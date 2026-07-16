# A* Search Trace Format (v1)

The contract between the C++ demo and the Python/HTML visualizers.
One trace file fully describes a search run, enough to replay it frame by frame.

## Design goals
- Self-contained: the grid, costs, start/goal, every expansion step, and the
  final path all in one file. The visualizer needs nothing else.
- Replayable: expansions are ordered, so the visualizer can animate the frontier
  growing one step at a time.
- Planner-agnostic: D* Lite and Field D* will emit the same schema (with an
  extra field or two), so all four articles share one visualizer.

## Schema

```json
{
  "meta": {
    "algorithm": "astar",
    "heuristic": "manhattan",
    "connectivity": 4,
    "width": 20,
    "height": 20
  },
  "grid": {
    "costs": [[1,1,1,...], ...],        // height rows x width cols, cell costs (>=1)
    "start": {"row": 17, "col": 2},
    "goal":  {"row": 2,  "col": 17}
  },
  "steps": [                             // ordered by expansion; step 0 is first
    {
      "expanded": {"row": 17, "col": 2}, // the node popped from the open set
      "g": 0,                            // cost-so-far at expansion
      "f": 30,                           // g + h at expansion
      "frontier": [                      // open-set snapshot AFTER this expansion
        {"row": 16, "col": 2, "f": 30},
        {"row": 17, "col": 3, "f": 30}
      ]
    }
    // ... one entry per expanded node
  ],
  "path": [                              // final path, start -> goal
    {"row": 17, "col": 2},
    {"row": 16, "col": 2},
    ...
    {"row": 2, "col": 17}
  ],
  "replans": [                           // Part 2+: empty/absent for a one-shot A* run
    {
      "origin": {"row": 12, "col": 16},  // where the rover is when it replans
      "changes": [                       // cells whose cost changed just before this replan
        {"row": 10, "col": 18, "cost": 1000000}
      ],
      "steps": [ /* same shape as top-level steps */ ],
      "path":  [ /* new path, origin -> goal */ ]
    }
    // ... one entry per replan during the drive
  ],
  "stats": {
    "expanded_count": 143,               // initial-phase expansions only
    "path_length": 31,
    "replan_count": 9
  }
}
```

## Field notes

- `grid.costs` is row-major: `costs[row][col]`. Obstacles are encoded as a large
  sentinel cost (e.g. 1000000) rather than a separate mask, so the visualizer can
  render them from the cost value alone. (Impassable = cost >= OBSTACLE_THRESHOLD.)
- `steps[i].frontier` is the open set *after* expanding `steps[i].expanded`. This
  lets the animation show the wave: expanded cells accumulate, frontier cells
  pulse at the leading edge.
- `path` is the reconstructed optimal path. Present only if the goal was reached;
  omitted or empty if no path exists.
- `replans` (Part 2+) records a sensor-limited drive: the top-level `steps`/`path`
  are the initial search from the start, then each replan entry is a fresh search
  from the rover's current cell (`origin`) after the listed `changes` were revealed.
  A one-shot A* run (Part 1) has an empty `replans`, so the schema is unchanged for
  it. We only record expansions (one `steps` entry per expanded node); D* Lite's
  vertex updates and its open set are deliberately not traced — the honest measure
  of work is the expansion count, and the open set stays large and dormant.

## Why frontier snapshots per step

Storing the frontier at every step is a little verbose, but it makes the
visualizer trivial: each animation frame is just "draw expanded[0..i] + frontier[i]".
No need to re-run the algorithm in Python. For a 20x20 grid the trace is a few
hundred KB, which is fine. For large grids we'd switch to frontier *deltas*
(added/removed) instead of full snapshots, but that optimization isn't needed yet.