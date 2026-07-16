#include "search_trace.h"
#include <sstream>

namespace {

// Minimal JSON helpers. We control the exact shape, so hand-writing is simpler
// and dependency-free. Indentation is cosmetic (traces are also machine-read).

std::string point_json(const Point& p) {
    std::ostringstream os;
    os << "{\"row\": " << p.row << ", \"col\": " << p.col << "}";
    return os.str();
}

// One search phase's steps + path, shared by the initial search and each replan.
void write_steps(std::ostream& out, const std::vector<SearchTrace::Step>& steps,
                 const char* indent) {
    out << "[";
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const auto& s = steps[i];
        out << (i == 0 ? "" : ",") << "\n" << indent << "  {\n";
        out << indent << "    \"expanded\": " << point_json(s.expanded) << ",\n";
        out << indent << "    \"g\": " << s.g << ",\n";
        out << indent << "    \"f\": " << s.f << ",\n";
        out << indent << "    \"frontier\": [";
        for (std::size_t k = 0; k < s.frontier.size(); ++k) {
            const auto& fe = s.frontier[k];
            out << (k == 0 ? "" : ", ")
                << "{\"row\": " << fe.p.row << ", \"col\": " << fe.p.col
                << ", \"f\": " << fe.f << "}";
        }
        out << "]\n" << indent << "  }";
    }
    out << (steps.empty() ? "" : (std::string("\n") + indent)) << "]";
}

void write_path(std::ostream& out, const std::vector<Point>& path, const char* indent) {
    out << "[";
    for (std::size_t i = 0; i < path.size(); ++i)
        out << (i == 0 ? "" : ", ") << "\n" << indent << "  " << point_json(path[i]);
    out << (path.empty() ? "" : (std::string("\n") + indent)) << "]";
}

}  // namespace

void SearchTrace::save(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out) throw std::runtime_error("Cannot open trace file for writing: " + filepath);

    out << "{\n";

    // --- meta ---
    out << "  \"meta\": {\n"
        << "    \"algorithm\": \"" << algo << "\",\n"
        << "    \"heuristic\": \"" << heur << "\",\n"
        << "    \"connectivity\": " << conn << ",\n"
        << "    \"width\": " << w << ",\n"
        << "    \"height\": " << h << "\n"
        << "  },\n";

    // --- grid ---
    out << "  \"grid\": {\n";
    out << "    \"costs\": [";
    for (int r = 0; r < h; ++r) {
        out << (r == 0 ? "" : ",") << "\n      [";
        for (int c = 0; c < w; ++c) {
            out << (c == 0 ? "" : ", ") << costs[r][c];
        }
        out << "]";
    }
    out << "\n    ],\n";
    out << "    \"start\": " << (start ? point_json(*start) : "null") << ",\n";
    out << "    \"goal\": "  << (goal  ? point_json(*goal)  : "null") << "\n";
    out << "  },\n";

    // --- steps (initial phase) ---
    out << "  \"steps\": ";
    write_steps(out, steps, "  ");
    out << ",\n";

    // --- path (initial phase) ---
    out << "  \"path\": ";
    write_path(out, path, "  ");
    out << ",\n";

    // --- replans (Part 2+): each is a fresh search after some cells changed ---
    out << "  \"replans\": [";
    for (std::size_t i = 0; i < replans.size(); ++i) {
        const Replan& rp = replans[i];
        out << (i == 0 ? "" : ",") << "\n    {\n";
        out << "      \"origin\": " << point_json(rp.origin) << ",\n";
        out << "      \"changes\": [";
        for (std::size_t k = 0; k < rp.changes.size(); ++k) {
            const auto& ch = rp.changes[k];
            out << (k == 0 ? "" : ", ")
                << "{\"row\": " << ch.p.row << ", \"col\": " << ch.p.col
                << ", \"cost\": " << ch.cost << "}";
        }
        out << "],\n";
        out << "      \"steps\": ";
        write_steps(out, rp.steps, "      ");
        out << ",\n";
        out << "      \"path\": ";
        write_path(out, rp.path, "      ");
        out << "\n    }";
    }
    out << (replans.empty() ? "" : "\n  ") << "],\n";

    // --- stats ---
    out << "  \"stats\": {\n"
        << "    \"expanded_count\": " << steps.size() << ",\n"
        << "    \"path_length\": " << path.size() << ",\n"
        << "    \"replan_count\": " << replans.size() << "\n"
        << "  }\n";

    out << "}\n";
}