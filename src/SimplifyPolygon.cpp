#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Raw CSV row from the input file.
struct CSVRow {
    int    ring_id;
    int    vertex_id;
    double x;
    double y;
};

struct Ring; // forward declaration needed by Vertex

// Node in a circular doubly-linked list.
struct Vertex {
    double x, y;

    Vertex* prev = nullptr;
    Vertex* next = nullptr;
    Ring*   ring = nullptr;

    bool alive = true;
    int  vid   = 0;
};

// Closed polygon ring stored as a circular doubly-linked list.
// ring_id == 0  →  exterior (CCW, positive signed area)
// ring_id  > 0  →  hole    (CW,  negative signed area)
struct Ring {
    int  rid;
    bool is_exterior;

private:
    int     size_        = 0;
    Vertex* head_        = nullptr;
    int     vid_counter_ = 0;

    mutable bool   area_valid_         = false;
    mutable double cached_signed_area_ = 0.0;

    std::vector<std::unique_ptr<Vertex>> storage_;

public:
    double original_area = 0.0;

    explicit Ring(int id,
                  const std::vector<std::pair<double, double>>& coords,
                  bool exterior = true)
        : rid(id), is_exterior(exterior)
    {
        auto pts = coords;
        if (pts.size() >= 2 && pts.front() == pts.back())
            pts.pop_back(); // strip duplicate closing vertex if present

        if (pts.size() < 3)
            throw std::invalid_argument("Ring must have at least 3 vertices.");

        int n = static_cast<int>(pts.size());
        storage_.reserve(n);
        for (auto& [x, y] : pts) {
            auto v   = std::make_unique<Vertex>();
            v->x     = x;
            v->y     = y;
            v->vid   = ++vid_counter_;
            v->ring  = this;
            storage_.push_back(std::move(v));
        }
        for (int i = 0; i < n; ++i) {
            storage_[i]->prev = storage_[(i + n - 1) % n].get();
            storage_[i]->next = storage_[(i + 1)     % n].get();
        }
        head_ = storage_[0].get();
        size_ = n;
    }

    int     size() const { return size_; }
    Vertex* head() const { return head_; }

    // Visit every vertex in ring order (circular traversal).
    template <typename F>
    void for_each(F&& f) const {
        Vertex* v = head_;
        do { f(v); v = v->next; } while (v != head_);
    }

    // Shoelace signed area: positive = CCW (exterior), negative = CW (hole).
    double signed_area(bool force = false) const {
        if (area_valid_ && !force) return cached_signed_area_;
        double s = 0.0;
        for_each([&](Vertex* v) {
            s += v->x * v->next->y - v->next->x * v->y;
        });
        cached_signed_area_ = s * 0.5;
        area_valid_         = true;
        return cached_signed_area_;
    }

    double area(bool force = false) const { return std::abs(signed_area(force)); }

    void invalidate_area_cache() { area_valid_ = false; }

    // Returns vertices as a closed coordinate list (first vertex repeated at end).
    std::vector<std::pair<double, double>> to_coords() const {
        std::vector<std::pair<double, double>> out;
        out.reserve(size_ + 1);
        for_each([&](Vertex* v) { out.emplace_back(v->x, v->y); });
        out.push_back(out.front());
        return out;
    }
};

// =========================================================================== //
//  Main Driver
// =========================================================================== //

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

    std::string line;
    std::getline(file, line); // skip header

    std::unordered_map<int, std::vector<std::pair<double, double>>> ring_data;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cell;

        CSVRow row;
        std::getline(ss, cell, ','); row.ring_id   = std::stoi(cell);
        std::getline(ss, cell, ','); row.vertex_id = std::stoi(cell);
        std::getline(ss, cell, ','); row.x         = std::stod(cell);
        std::getline(ss, cell, ','); row.y         = std::stod(cell);

        ring_data[row.ring_id].emplace_back(row.x, row.y);
    }

    std::vector<std::unique_ptr<Ring>> owned_rings;
    for (auto& [rid, coords] : ring_data)
        owned_rings.push_back(std::make_unique<Ring>(rid, coords, rid == 0));

    std::sort(owned_rings.begin(), owned_rings.end(),
              [](const auto& a, const auto& b) { return a->rid < b->rid; });

    // Compute total signed area of the input.
    double initial_signed_area = 0.0;
    for (auto& r : owned_rings)
        initial_signed_area += r->signed_area();

    // Pass-through output — no simplification yet.
    std::cout << "ring_id,vertex_id,x,y\n";
    std::cout << std::defaultfloat;

    for (auto& r : owned_rings) {
        auto coords = r->to_coords();
        size_t n    = coords.size() - 1; // exclude repeated closing vertex
        for (size_t i = 0; i < n; ++i)
            std::cout << r->rid << "," << i << ","
                      << coords[i].first << "," << coords[i].second << "\n";
    }

    double final_signed_area = initial_signed_area; // temp

    std::cout << std::scientific << std::setprecision(6);
    std::cout << "Total signed area in input: "  << initial_signed_area << "\n";
    std::cout << "Total signed area in output: " << final_signed_area   << "\n";
    std::cout << "Total areal displacement: "    << 0.0                 << "\n";

    return 0;
}
