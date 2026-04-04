# CSD2183 T5 Group 03 — Area-and-Topology-Preserving Polygon Simplification

Implementation of the **Area-Preserving Segment Collapse (APSC)** algorithm from:

> Kronenfeld et al. (2020), *Simplification of Polylines by the Segment Collapse Method*, Cartography and Geographic Information Science.

---

## Algorithm

The simplifier uses a **global greedy priority queue** across all rings. At each step it collapses the vertex whose removal introduces the smallest triangle area. After all collapses a **per-ring Newton area-correction pass** shifts the vertex with the longest opposite base perpendicularly until each ring's signed area matches the original to floating-point tolerance. Candidate vertices for correction are tried in descending base-length order; any correction that would create a topological violation is skipped.

Key data structures:
- **Circular doubly-linked list (DCEL)** per ring for O(1) vertex removal
- **Lazy-deletion min-heap** (versioned entries) for the global priority queue
- **Uniform-grid spatial index** for O(1) average-case topology checks

---

## Repository layout

```
.
├── makefile
├── src/
│   ├── SimplifyPolygon.cpp # Main simplification program
│   └── validate.cpp        # Property-based output validator
└── test_cases/
    ├── input_*.csv         # Input polygons (ring_id, vertex_id, x, y)
    └── output_*.txt        # Reference outputs
```

---

## Build

### Linux / WSL (GNU make + g++)

```bash
make           # builds ./simplify and ./validate
make clean
```

---

## Usage

```bash
./simplify <input_file.csv> <target_vertices>
```

Input CSV columns: `ring_id, vertex_id, x, y`  
Output: simplified CSV to stdout followed by summary metrics.

**Example:**
```bash
./simplify test_cases/input_original_01.csv 99
```

---

## Testing

### Property-based validation

```bash
make check
```

Pipes each test case through `./validate`, which checks:

| Check | Criterion |
|---|---|
| Ring count | Same number of rings as input |
| Area per ring | Each ring's area preserved to 1 × 10⁻⁶ relative tolerance |
| Topology | No self-intersections; no cross-ring intersections |
| Orientations | Exterior ring CCW, interior rings CW |
| Vertex count | ≤ target, or no further valid greedy removal possible |
| Geometric consistency | Signed area recomputed from output coordinates matches reported value |
| Displacement | Non-negative and finite |

### Reference diff (exact output match)

```bash
make test
```

---

## Output format

```
ring_id,vertex_id,x,y
0,0,...
...
Total signed area in input:  <value>
Total signed area in output: <value>
Total areal displacement:    <value>
```

Coordinates are output with 15 significant digits to ensure geometric consistency between the reported signed area and the value recomputable from the coordinates.
