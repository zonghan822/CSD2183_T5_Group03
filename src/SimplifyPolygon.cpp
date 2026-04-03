#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
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

struct Ring;

struct Vertex {
    double x, y;

    Vertex* prev = nullptr;
    Vertex* next = nullptr;
    Ring*   ring = nullptr;

    bool alive = true;
    int  vid   = 0;

    int current_version = 0;

    mutable bool   area_valid  = false;
    mutable double cached_area = 0.0;

    double triangle_area() const {
        if (area_valid) return cached_area;
        double ax = prev->x, ay = prev->y;
        double bx = x,       by = y;
        double cx = next->x, cy = next->y;
        cached_area = 0.5 * ((bx - ax) * (cy - ay) - (cx - ax) * (by - ay));
        area_valid  = true;
        return cached_area;
    }

    void invalidate_cache() const { area_valid = false; }
};

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
        // TODO: add minimum-size and closing-vertex guards (deferred)
        int n = static_cast<int>(coords.size());
        storage_.reserve(n);
        for (auto& [x, y] : coords) {
            auto v  = std::make_unique<Vertex>();
            v->x    = x;
            v->y    = y;
            v->vid  = ++vid_counter_;
            v->ring = this;
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

    template <typename F>
    void for_each(F&& f) const {
        Vertex* v = head_;
        do { f(v); v = v->next; } while (v != head_);
    }

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

    // Remove vertex v from the ring; returns its former neighbours.
    std::pair<Vertex*, Vertex*> collapse_vertex(Vertex* v) {
        if (size_ <= 3)
            throw std::runtime_error("Cannot collapse: ring would degenerate.");

        Vertex* p = v->prev;
        Vertex* n = v->next;

        p->next = n;
        n->prev = p;

        p->invalidate_cache();
        n->invalidate_cache();

        v->alive = false;
        v->prev  = nullptr;
        v->next  = nullptr;
        v->ring  = nullptr;

        if (head_ == v) head_ = n;
        --size_;
        invalidate_area_cache();
        return {p, n};
    }

    std::vector<std::pair<double, double>> to_coords() const {
        std::vector<std::pair<double, double>> out;
        out.reserve(size_ + 1);
        for_each([&](Vertex* v) { out.emplace_back(v->x, v->y); });
        out.push_back(out.front());
        return out;
    }
};

// =========================================================================== //
//  Greedy simplifier — global min-heap, no topology check yet
// =========================================================================== //

class GreedySimplifier {
    std::vector<Ring*> rings_;
    int                target_vertices_;
    double             total_displacement_ = 0.0;

    struct HeapEntry {
        double  cost;
        int     version;
        Vertex* v;
        bool operator>(const HeapEntry& o) const { return cost > o.cost; }
    };
    std::priority_queue<HeapEntry,
                        std::vector<HeapEntry>,
                        std::greater<HeapEntry>> pq_;

    void push_vertex(Vertex* v) {
        if (!v->alive || v->ring->size() <= 3) return;
        v->invalidate_cache();
        ++v->current_version;
        pq_.push({std::abs(v->triangle_area()), v->current_version, v});
    }

public:
    GreedySimplifier(std::vector<Ring*> rings, int target)
        : rings_(std::move(rings)), target_vertices_(target)
    {
        for (auto* r : rings_)
            r->for_each([&](Vertex* v) { push_vertex(v); });
    }

    void simplify() {
        int total = 0;
        for (auto* r : rings_) total += r->size();

        while (total > target_vertices_ && !pq_.empty()) {
            auto [cost, ver, v] = pq_.top();
            pq_.pop();

            if (!v->alive || ver != v->current_version || v->ring->size() <= 3)
                continue;

            total_displacement_ += std::abs(v->triangle_area());
            auto [p, n] = v->ring->collapse_vertex(v);
            push_vertex(p);
            push_vertex(n);
            --total;
        }
    }

    double total_displacement() const { return total_displacement_; }
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

    std::vector<Ring*> all_rings;
    for (auto& r : owned_rings) all_rings.push_back(r.get());

    double initial_signed_area = 0.0;
    for (auto* r : all_rings) initial_signed_area += r->signed_area();

    GreedySimplifier simplifier(all_rings, target_vertices);
    simplifier.simplify();

    std::cout << "ring_id,vertex_id,x,y\n";
    std::cout << std::defaultfloat;

    double final_signed_area = 0.0;
    for (auto* r : all_rings) {
        auto coords = r->to_coords();
        final_signed_area += r->signed_area(true);
        size_t n = coords.size() - 1;
        for (size_t i = 0; i < n; ++i)
            std::cout << r->rid << "," << i << ","
                      << coords[i].first << "," << coords[i].second << "\n";
    }

    std::cout << std::scientific << std::setprecision(6);
    std::cout << "Total signed area in input: "  << initial_signed_area             << "\n";
    std::cout << "Total signed area in output: " << final_signed_area               << "\n";
    std::cout << "Total areal displacement: "    << simplifier.total_displacement() << "\n";

    return 0;
}
