# space-path-planning
# Space Path Planning

**Path planning algorithms used by Mars rovers, implemented from scratch, explained, and visualized.**

![A* searching across Mars-like terrain](docs/images/astar_hero.png)

---

If you're like me, you're a little bit obsessed with the moment in space exploration we're living through right now. Humans flew around the Moon again this year for the first time in half a century. It's an extraordinary time to be paying attention.

A few months ago I came across the news that JPL had used AI to help plan a Mars rover's drive, and it stuck with me. It sat at the exact intersection of the things I love: space, robotics, AI. And I thought *this would make a great topic to dig into and write about.* This series is what came out of that.

So this repository is me going deep on how it works and sharing it. It's the code behind my blog series **[From A\* to Anthropic](#)**, which traces the nearly 60 years of teaching robots to find their way, from the 1968 A\* algorithm to the AI-assisted planning that drove Perseverance across Jezero Crater. Each article builds up an algorithm; this repo is where they actually run.

This is a passion project driven by the joy of understanding how a robot 225 million kilometres away decides where to put its wheels, and the fun of sharing that with anyone else who looks up at Mars and wonders.

---

## The series

Path planning on Mars is a beautiful problem. You can't steer a rover in real time because radio takes minutes to get there. So it has to be handed a plan and trusted to execute it across terrain it's only partly seen. That single constraint gave rise to a whole family of algorithms, each fixing a limitation of the last.

The plan is to work through them in order:

| Part | Algorithm | The idea it adds | Status |
|------|-----------|------------------|--------|
| 1 | **A\*** | A heuristic to explore toward the goal | ✅ done |
| 2 | **D\* / D\* Lite** | Replan *efficiently* when the map changes as you drive | 🚧 next |
| 3 | **Field D\*** | *Any-angle* paths; the version that actually flew on Mars | 📋 planned |
| 4 | **ENav + AI** | Modern planning, and where the AI-assisted drives fit in | 📋 planned |

Each part is a standalone article and a runnable piece of code, with visualizations you can regenerate yourself.

---

## What's here

The project has two halves that talk to each other through a simple JSON contract:

- A **C++ core** - the planners themselves, written as clean, templated, tested modern C++. Each planner works on any map that can answer "what are this cell's neighbours, and what does each step cost?", whether that's a grid laid over Martian terrain or an abstract graph.
- A **Python visualizer** - turns a search into a hero image and an animated GIF of the frontier expanding. This is where the algorithms become something you can *see*.

The bridge between them is a **search trace**: the C++ demo records every step of a search into a JSON file, and the visualizer renders it. One format, every algorithm, all four articles.

```
├── include/          # header-only templated core (planners, maps, Point, heuristics)
│   ├── core/         #   Point, PriorityList, PathPlanner, heuristics, SearchTrace
│   ├── maps/         #   Grid and Graph: interchangeable map types
│   └── planners/     #   A*, and D*/D* Lite/Field D* as they land
├── src/              # non-templated support code
├── apps/             # one runnable demo per article; builds a map, runs a planner, writes a trace
├── tests/            # GoogleTest coverage
├── tools/visualizer/ # the Python side: trace → images, GIFs
├── traces/           # generated search traces (one sample committed so it runs on clone)
└── docs/             # the articles, the trace-format spec, images
```

---

## Try it

**The C++ side** (planners, demos, tests):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

ctest --test-dir build --output-on-failure      # run the tests
./build/apps/astar_demo traces/run.json          # run A*, write a trace
```

Tests are pulled in automatically via GoogleTest.

**The visualizer** (turns a trace into visuals):

```bash
cd tools/visualizer
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

# hero image + animation from a trace
python render_trace.py ../../traces/run.json --hero hero.png --anim search.gif
```

Want to see it move without building the C++ first? A sample trace ships with the repo:

```bash
python render_trace.py ../../traces/sample_astar.json --anim search.gif
```

---

## A note on the visuals

Everything you see comes from a real search on procedurally generated Mars-like terrain. Dark cells are cheap regolith, orange is increasingly rocky, black is impassable. The blockiness of the path isn't a bug; it's a direct consequence of only letting the rover move in four directions, and it's exactly the limitation that later algorithms in the series set out to fix. (More on that in part 3.)


## License

MIT. Use it, learn from it, build on it. If it's useful to you, I'd love to hear about it.

---

*Built with a lot of enthusiasm 🔭*