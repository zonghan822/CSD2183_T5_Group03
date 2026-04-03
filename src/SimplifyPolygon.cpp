/**
 * SimplifyPolygon.cpp — Globally Optimized Area-Preserving Segment Collapse (APSC)
 * 
 * Reference: Kronenfeld et al. (2020), "Simplification of Polylines by the
 * Segment Collapse Method", Cartography and Geographic Information Science.

 * Usage:
 * ./simplify <input_file.csv> <target_vertices>
 *
 * This algorithm uses a global priority queue across all rings and tracks
 * accumulated symmetric difference error to minimize Total Areal Displacement,
 * while maintaining the precise area-correction pass to ensure exact input/output
 * area equivalence.
 */

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
#include <unordered_set>
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

    int    current_version   = 0;
    double accumulated_error = 0.0;

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

    double segment_length() const {
        double dx = next->x - x, dy = next->y - y;
        return std::hypot(dx, dy);
    }
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
        auto pts = coords;
        if (pts.size() >= 2 && pts.front() == pts.back())
            pts.pop_back();

        if (pts.size() < 3)
            throw std::invalid_argument("Ring must have at least 3 vertices.");

        int n = static_cast<int>(pts.size());

        storage_.reserve(n);
        for (auto& [x, y] : pts) {
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
//  Geometric predicates
// =========================================================================== //

static double cross2(double ox, double oy, double ax, double ay, double bx, double by) {
    return (ax - ox) * (by - oy) - (bx - ox) * (ay - oy);
}

static bool on_segment(double px, double py, double ax, double ay, double bx, double by) {
    return std::min(ax, bx) <= px && px <= std::max(ax, bx) &&
           std::min(ay, by) <= py && py <= std::max(ay, by);
}

static bool segments_intersect(const Vertex* a, const Vertex* b,
                                const Vertex* c, const Vertex* d) {
    double ax=a->x, ay=a->y, bx=b->x, by=b->y;
    double cx=c->x, cy=c->y, dx=d->x, dy=d->y;
    double d1=cross2(cx,cy,dx,dy,ax,ay), d2=cross2(cx,cy,dx,dy,bx,by);
    double d3=cross2(ax,ay,bx,by,cx,cy), d4=cross2(ax,ay,bx,by,dx,dy);
    if (((d1>0&&d2<0)||(d1<0&&d2>0)) && ((d3>0&&d4<0)||(d3<0&&d4>0))) return true;
    int shared = ((a->vid==c->vid||a->vid==d->vid)?1:0)
               + ((b->vid==c->vid||b->vid==d->vid)?1:0);
    if (shared >= 1) return false;
    if (d1==0 && on_segment(ax,ay,cx,cy,dx,dy)) return true;
    if (d2==0 && on_segment(bx,by,cx,cy,dx,dy)) return true;
    if (d3==0 && on_segment(cx,cy,ax,ay,bx,by)) return true;
    if (d4==0 && on_segment(dx,dy,ax,ay,bx,by)) return true;
    return false;
}

// =========================================================================== //
//  Spatial index — uniform grid
// =========================================================================== //

struct SegKey {
    int va, vb;
    bool operator==(const SegKey& o) const { return va==o.va && vb==o.vb; }
};
struct SegKeyHash {
    std::size_t operator()(const SegKey& s) const {
        return std::hash<long long>{}((static_cast<long long>(s.va) << 32) | static_cast<unsigned>(s.vb));
    }
};
struct CellKey {
    int col, row;
    bool operator==(const CellKey& o) const { return col==o.col && row==o.row; }
};
struct CellKeyHash {
    std::size_t operator()(const CellKey& c) const {
        return std::hash<long long>{}((static_cast<long long>(c.col) << 32) | static_cast<unsigned>(c.row));
    }
};

class GridIndex {
    double minx_, miny_, cell_w_, cell_h_;

    std::unordered_map<CellKey,
                       std::unordered_set<SegKey, SegKeyHash>,
                       CellKeyHash> grid_;

    struct SegInfo { Vertex* a; Vertex* b; std::vector<CellKey> cells; };
    std::unordered_map<long long, SegInfo> seg_map_;

    static long long encode(int va, int vb) {
        return (static_cast<long long>(va) << 32) | static_cast<unsigned>(vb);
    }

    std::vector<CellKey> cells_for(double x0, double y0,
                                   double x1, double y1) const {
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        int c0 = std::max(0, int((x0 - minx_) / cell_w_));
        int c1 = std::max(0, int((x1 - minx_) / cell_w_));
        int r0 = std::max(0, int((y0 - miny_) / cell_h_));
        int r1 = std::max(0, int((y1 - miny_) / cell_h_));
        std::vector<CellKey> out;
        out.reserve((c1-c0+1) * (r1-r0+1));
        for (int c = c0; c <= c1; ++c)
            for (int r = r0; r <= r1; ++r)
                out.push_back({c, r});
        return out;
    }

public:
    GridIndex(double mnx, double mny, double mxx, double mxy, int n_hint)
        : minx_(mnx), miny_(mny)
    {
        double w = std::max(mxx - mnx, 1e-9);
        double h = std::max(mxy - mny, 1e-9);
        int cpa  = std::max(4, int(std::sqrt(double(n_hint))));
        cell_w_  = w / cpa;
        cell_h_  = h / cpa;
    }

    void insert(Vertex* a, Vertex* b) {
        long long key   = encode(a->vid, b->vid);
        auto      cells = cells_for(a->x, a->y, b->x, b->y);
        SegKey    sk{a->vid, b->vid};
        for (auto& c : cells) grid_[c].insert(sk);
        seg_map_[key] = {a, b, std::move(cells)};
    }

    void remove(Vertex* a, Vertex* b) {
        long long key = encode(a->vid, b->vid);
        auto it = seg_map_.find(key);
        if (it == seg_map_.end()) return;
        SegKey sk{a->vid, b->vid};
        for (auto& c : it->second.cells) {
            auto git = grid_.find(c);
            if (git != grid_.end()) {
                git->second.erase(sk);
                if (git->second.empty()) grid_.erase(git);
            }
        }
        seg_map_.erase(it);
    }

    bool intersects_any(Vertex* a, Vertex* b) const {
        auto cells = cells_for(a->x, a->y, b->x, b->y);
        std::unordered_set<long long> visited;
        for (auto& cell : cells) {
            auto git = grid_.find(cell);
            if (git == grid_.end()) continue;
            for (auto& sk : git->second) {
                long long k = encode(sk.va, sk.vb);
                if (!visited.insert(k).second) continue;
                auto sit = seg_map_.find(k);
                if (sit == seg_map_.end()) continue;
                if (segments_intersect(a, b, sit->second.a, sit->second.b))
                    return true;
            }
        }
        return false;
    }
};

class PolygonIndex {
    std::unique_ptr<GridIndex> idx_;
public:
    explicit PolygonIndex(const std::vector<Ring*>& rings) {
        double mnx=1e18, mny=1e18, mxx=-1e18, mxy=-1e18;
        int ns = 0;
        for (auto* r : rings)
            r->for_each([&](Vertex* v) {
                mnx = std::min(mnx, v->x); mny = std::min(mny, v->y);
                mxx = std::max(mxx, v->x); mxy = std::max(mxy, v->y);
                ++ns;
            });
        idx_ = std::make_unique<GridIndex>(mnx, mny, mxx, mxy, ns);
        for (auto* r : rings)
            r->for_each([&](Vertex* v) { idx_->insert(v, v->next); });
    }

    void remove_vertex_edges(Vertex* v) {
        idx_->remove(v->prev, v);
        idx_->remove(v, v->next);
    }
    void add_new_edge(Vertex* p, Vertex* n)   { idx_->insert(p, n); }
    void restore_vertex_edges(Vertex* v) {
        idx_->insert(v->prev, v);
        idx_->insert(v, v->next);
    }
    bool new_edge_valid(Vertex* p, Vertex* n) const {
        return !idx_->intersects_any(p, n);
    }
};

// =========================================================================== //
//  Global APSC simplifier
// =========================================================================== //

class GlobalAPSCSimplifier {
    std::vector<Ring*>             rings_;
    int                            target_vertices_;
    std::unique_ptr<PolygonIndex>  spatial_;
    std::unordered_map<int,double> orig_area_;
    double                         total_disp_ = 0.0;

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

        double base_cost = std::abs(v->triangle_area());

        double dx1 = v->x - v->prev->x, dy1 = v->y - v->prev->y;
        double dx2 = v->next->x - v->x, dy2 = v->next->y - v->y;
        double dot = dx1*dx2 + dy1*dy2;
        double len1 = std::hypot(dx1, dy1), len2 = std::hypot(dx2, dy2);
        double cos_theta = dot / (len1 * len2 + 1e-9);
        double penalty   = (cos_theta < -0.5) ? 2.0 : 1.0;

        double final_cost = (base_cost + v->accumulated_error) * penalty;

        ++v->current_version;
        pq_.push({final_cost, v->current_version, v});
    }

    // Iterative Newton correction
    void correct_area(Ring* ring) {
        double target        = orig_area_.at(ring->rid);
        double target_signed = ring->is_exterior ? target : -target;

        for (int iter = 0; iter < 10; ++iter) {
            double current = ring->signed_area(true);
            double error   = current - target_signed;
            if (std::abs(error) < 1e-10 * target) return;

            Vertex* best_v = nullptr;
            double  best_L = 0.0;
            ring->for_each([&](Vertex* v) {
                double dx = v->next->x - v->prev->x;
                double dy = v->next->y - v->prev->y;
                double L  = std::hypot(dx, dy);
                if (L > best_L) { best_L = L; best_v = v; }
            });
            if (!best_v || best_L < 1e-12) return;

            double dx    = best_v->next->x - best_v->prev->x;
            double dy    = best_v->next->y - best_v->prev->y;
            double L     = best_L;
            double delta = 2.0 * error / L;
            best_v->x   += (-dy / L) * delta;
            best_v->y   += ( dx / L) * delta;
            best_v->invalidate_cache();
            ring->invalidate_area_cache();
        }
    }

public:
    GlobalAPSCSimplifier(std::vector<Ring*> rings, int target)
        : rings_(rings), target_vertices_(target)
    {
        spatial_ = std::make_unique<PolygonIndex>(rings_);
        for (auto* r : rings_) {
            orig_area_[r->rid]  = r->area();
            r->original_area    = orig_area_[r->rid];
            r->for_each([&](Vertex* v) {
                v->accumulated_error = 0.0;
                push_vertex(v);
            });
        }
    }

    void simplify() {
        int total = 0;
        for (auto* r : rings_) total += r->size();

        while (total > target_vertices_ && !pq_.empty()) {
            auto [cost, ver, v] = pq_.top();
            pq_.pop();

            if (!v->alive || ver != v->current_version || v->ring->size() <= 3)
                continue;

            Vertex* p = v->prev;
            Vertex* n = v->next;

            spatial_->remove_vertex_edges(v);
            bool valid = spatial_->new_edge_valid(p, n);

            if (valid) {
                double step_disp = std::abs(v->triangle_area());
                total_disp_ += step_disp;
                v->ring->collapse_vertex(v);
                spatial_->add_new_edge(p, n);
                // Propagate half the displacement to each neighbour
                p->accumulated_error += step_disp * 0.5;
                n->accumulated_error += step_disp * 0.5;
                push_vertex(p);
                push_vertex(n);
                --total;
            } else {
                spatial_->restore_vertex_edges(v);
            }
        }

        for (auto* r : rings_) correct_area(r);
    }

    double total_displacement() const { return total_disp_; }
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

    GlobalAPSCSimplifier simplifier(all_rings, target_vertices);
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
    std::cout << "Total signed area in input: "  << initial_signed_area          << "\n";
    std::cout << "Total signed area in output: " << final_signed_area            << "\n";
    std::cout << "Total areal displacement: "    << simplifier.total_displacement() << "\n";

    return 0;
}
