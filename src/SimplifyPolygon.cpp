#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Raw CSV row from the input file.
struct CSVRow {
    int    ring_id;
    int    vertex_id;
    double x;
    double y;
};

// A single 2-D point.
struct Vertex {
    int    vid;
    double x, y;
};

// An ordered list of vertices forming one closed ring.
// ring_id == 0  →  exterior ring (CCW)
// ring_id  > 0  →  hole (CW)
struct Ring {
    int                 rid;
    std::vector<Vertex> vertices;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <input_file.csv> <target_vertices>\n";
        return 1;
    }

    const std::string filename        = argv[1];
    const int         target_vertices = std::stoi(argv[2]);
    (void)target_vertices; // not used yet

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << "\n";
        return 1;
    }

    // ------------------------------------------------------------------ //
    //  Parse CSV
    // ------------------------------------------------------------------ //
    std::string line;
    std::getline(file, line); // skip header

    // std::map keeps rings sorted by ring_id automatically.
    std::map<int, Ring> ring_map;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cell;

        CSVRow row;
        std::getline(ss, cell, ','); row.ring_id   = std::stoi(cell);
        std::getline(ss, cell, ','); row.vertex_id = std::stoi(cell);
        std::getline(ss, cell, ','); row.x         = std::stod(cell);
        std::getline(ss, cell, ','); row.y         = std::stod(cell);

        ring_map[row.ring_id].rid = row.ring_id;
        ring_map[row.ring_id].vertices.push_back({row.vertex_id, row.x, row.y});
    }

    // ------------------------------------------------------------------ //
    //  Output — pass-through, no simplification yet
    // ------------------------------------------------------------------ //
    std::cout << "ring_id,vertex_id,x,y\n";

    for (auto& [rid, ring] : ring_map) {
        int new_vid = 0;
        for (auto& v : ring.vertices)
            std::cout << rid << "," << new_vid++ << ","
                      << v.x << "," << v.y << "\n";
    }

    // Area computation not implemented yet — placeholder zeroes.
    std::cout << std::scientific << std::setprecision(6);
    std::cout << "Total signed area in input: "  << 0.0 << "\n";
    std::cout << "Total signed area in output: " << 0.0 << "\n";
    std::cout << "Total areal displacement: "    << 0.0 << "\n";

    return 0;
}
