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

    // Cached signed triangle area
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

    // APSC segment collapse: replace the B→C edge with Steiner point E.
    // The sequence A→B→C→D becomes A→E→D; C is marked dead.
    void collapse_segment(Vertex* b, Vertex* c, double ex, double ey) {
        Vertex* a = b->prev;
        Vertex* d = c->next;

        b->x    = ex; // B is repurposed as the new Steiner point E
        b->y    = ey;
        b->next = d;
        d->prev = b;

        c->alive = false; // C is removed from the ring

        a->invalidate_cache();
        b->invalidate_cache();
        d->invalidate_cache();

        if (head_ == c) head_ = b;
        --size_;
        invalidate_area_cache();
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
//  Geometry helpers for Steiner-point placement (Kronenfeld 2020)
// =========================================================================== //

// Perpendicular distance from point P to line AB.
static double point_to_line_dist(double px, double py,
                                  double ax, double ay,
                                  double bx, double by) {
    double num  = std::abs((by - ay) * px - (bx - ax) * py + bx * ay - by * ax);
    double denom = std::hypot(bx - ax, by - ay);
    return (denom < 1e-12) ? 0.0 : num / denom;
}

// Intersection of line (p1, dir v1) with line (p2, dir v2).
static std::pair<double, double> line_intersect(double x1, double y1,
                                                 double vx1, double vy1,
                                                 double x2, double y2,
                                                 double vx2, double vy2) {
    double det = vx1 * vy2 - vy1 * vx2;
    if (std::abs(det) < 1e-9) return {x1, y1}; // parallel fallback
    double t = (vy1 * (x2 - x1) - vx1 * (y2 - y1)) / det;
    return {x2 + t * vx2, y2 + t * vy2};
}

// =========================================================================== //
//  Collapse priority queue (lazy-deletion min-heap)
// =========================================================================== //

class CollapseQueue {
    struct Entry {
        double  key;
        int     version;
        Vertex* v;
        bool operator>(const Entry& o) const { return key > o.key; }
    };
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap_;

public:
    void push(Vertex* v) {
        v->invalidate_cache();
        ++v->current_version;
        heap_.push({std::abs(v->triangle_area()), v->current_version, v});
    }

    Vertex* pop() {
        while (!heap_.empty()) {
            auto [key, ver, v] = heap_.top();
            heap_.pop();
            if (v->alive && ver == v->current_version) return v;
        }
        return nullptr;
    }

    bool empty() const { return heap_.empty(); }
};

// =========================================================================== //
//  APSC simplifier — per-ring Steiner collapse
// =========================================================================== //

class APSCSimplifier {
    std::vector<Ring*> rings_;
    int                target_vertices_;
    double             total_displacement_ = 0.0;

public:
    APSCSimplifier(std::vector<Ring*> rings, int target)
        : rings_(std::move(rings)), target_vertices_(target) {}

    void simplify() {
        int total = 0;
        for (auto* r : rings_) total += r->size();

        for (auto* r : rings_) {
            CollapseQueue q;
            r->for_each([&](Vertex* v) { q.push(v); });

            while (r->size() > 3 && total > target_vertices_) {
                Vertex* b = q.pop();
                if (!b) break;

                Vertex* a = b->prev;
                Vertex* c = b->next;
                Vertex* d = c->next;

                // Signed area of quadrilateral ABCD (= area to preserve).
                double area_abc = 0.5 * ((b->x-a->x)*(c->y-a->y) - (c->x-a->x)*(b->y-a->y));
                double area_acd = 0.5 * ((c->x-a->x)*(d->y-a->y) - (d->x-a->x)*(c->y-a->y));
                double quad_area = area_abc + area_acd;

                double dx = d->x - a->x, dy = d->y - a->y;
                double len_ad = std::hypot(dx, dy);

                double ex, ey;
                if (len_ad < 1e-9) {
                    ex = d->x; ey = d->y;
                } else {
                    // E lies on line E↔ parallel to AD at signed height h.
                    double h  = (2.0 * quad_area) / len_ad;
                    double nx = -dy / len_ad, ny = dx / len_ad; // unit normal
                    double px = a->x + h * nx, py = a->y + h * ny; // point on E↔

                    // If BC is parallel to AD, use the closer endpoint of BC.
                    double dot_bc_ad = (c->x - b->x) * dx + (c->y - b->y) * dy;
                    double len_bc    = std::hypot(c->x - b->x, c->y - b->y);
                    bool   parallel  = (len_bc > 1e-9) &&
                        (std::abs(std::abs(dot_bc_ad / (len_ad * len_bc)) - 1.0) < 1e-5);

                    if (parallel) {
                        double db = point_to_line_dist(b->x, b->y, a->x, a->y, d->x, d->y);
                        double dc = point_to_line_dist(c->x, c->y, a->x, a->y, d->x, d->y);
                        if (db <= dc) {
                            auto pt = line_intersect(px, py, dx, dy,
                                                     a->x, a->y, b->x-a->x, b->y-a->y);
                            ex = pt.first; ey = pt.second;
                        } else {
                            auto pt = line_intersect(px, py, dx, dy,
                                                     c->x, c->y, d->x-c->x, d->y-c->y);
                            ex = pt.first; ey = pt.second;
                        }
                    } else {
                        // TODO: non-parallel case — midpoint heuristic is incorrect;
                        // proper placement requires a side test (Kronenfeld).
                        ex = px + dx * 0.5;
                        ey = py + dy * 0.5;
                    }
                }

                r->collapse_segment(b, c, ex, ey);
                total_displacement_ += std::abs(quad_area);
                --total;

                if (a->alive) q.push(a);
                if (b->alive) q.push(b);
                if (d->alive) q.push(d);
            }
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

    APSCSimplifier simplifier(all_rings, target_vertices);
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
