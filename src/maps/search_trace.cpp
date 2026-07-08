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

    // --- steps ---
    out << "  \"steps\": [";
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const Step& s = steps[i];
        out << (i == 0 ? "" : ",") << "\n    {\n";
        out << "      \"expanded\": " << point_json(s.expanded) << ",\n";
        out << "      \"g\": " << s.g << ",\n";
        out << "      \"f\": " << s.f << ",\n";
        out << "      \"frontier\": [";
        for (std::size_t k = 0; k < s.frontier.size(); ++k) {
            const auto& fe = s.frontier[k];
            out << (k == 0 ? "" : ", ")
                << "{\"row\": " << fe.p.row << ", \"col\": " << fe.p.col
                << ", \"f\": " << fe.f << "}";
        }
        out << "]\n      }";
    }
    out << "\n  ],\n";

    // --- path ---
    out << "  \"path\": [";
    for (std::size_t i = 0; i < path.size(); ++i) {
        out << (i == 0 ? "" : ", ") << "\n    " << point_json(path[i]);
    }
    out << (path.empty() ? "" : "\n  ") << "],\n";

    // --- stats ---
    int path_cost = 0;
    out << "  \"stats\": {\n"
        << "    \"expanded_count\": " << steps.size() << ",\n"
        << "    \"path_length\": " << path.size() << ",\n"
        << "    \"path_cost\": " << path_cost << "\n"
        << "  }\n";

    out << "}\n";
}