#pragma once

#include <cstdint>
#include <cassert>
#include "fixed_span.h"
#include <Imgui.h>

// Source - https://stackoverflow.com/a/14853492
// Posted by aggsol, modified by community. See post 'Timeline' for change history
// Retrieved 2026-06-28, License - CC BY-SA 4.0
inline uint32_t calcZOrder(uint16_t xPos, uint16_t yPos)
{
    static const uint32_t MASKS[] = { 0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF };
    static const uint32_t SHIFTS[] = { 1, 2, 4, 8 };

    uint32_t x = xPos;
    uint32_t y = yPos;

    x = (x | (x << SHIFTS[3])) & MASKS[3];
    x = (x | (x << SHIFTS[2])) & MASKS[2];
    x = (x | (x << SHIFTS[1])) & MASKS[1];
    x = (x | (x << SHIFTS[0])) & MASKS[0];

    y = (y | (y << SHIFTS[3])) & MASKS[3];
    y = (y | (y << SHIFTS[2])) & MASKS[2];
    y = (y | (y << SHIFTS[1])) & MASKS[1];
    y = (y | (y << SHIFTS[0])) & MASKS[0];

    return x | (y << 1);
}

inline uint16_t mortonToX(uint32_t morton)
{
    static const uint32_t MASKS[] = { 0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF };
    uint32_t x = morton & MASKS[0];
    x = (x | (x >> 1)) & MASKS[1];
    x = (x | (x >> 2)) & MASKS[2];
    x = (x | (x >> 4)) & MASKS[3];
    x = (x | (x >> 8)) & 0x0000FFFF;
    return (uint16_t)x;
}

inline uint16_t mortonToY(uint32_t morton)
{
    static const uint32_t MASKS[] = { 0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF };
    uint32_t y = (morton >> 1) & MASKS[0];
    y = (y | (y >> 1)) & MASKS[1];
    y = (y | (y >> 2)) & MASKS[2];
    y = (y | (y >> 4)) & MASKS[3];
    y = (y | (y >> 8)) & 0x0000FFFF;
    return (uint16_t)y;
}

static const uint32_t X_MASK = 0x55555555;
static const uint32_t Y_MASK = 0xAAAAAAAA;

inline uint32_t mortonIncX(uint32_t morton) { return ((morton | Y_MASK) + 1) & X_MASK | (morton & Y_MASK); }
inline uint32_t mortonDecX(uint32_t morton) { return ((morton & X_MASK) - 1) & X_MASK | (morton & Y_MASK); }
inline uint32_t mortonIncY(uint32_t morton) { return ((morton | X_MASK) + 2) & Y_MASK | (morton & X_MASK); }
inline uint32_t mortonDecY(uint32_t morton) { return ((morton & Y_MASK) - 2) & Y_MASK | (morton & X_MASK); }

inline void mortonNeighbours3x3(uint32_t morton, uint32_t out[9])
{
    uint32_t ym1 = mortonDecY(morton);
    uint32_t yp1 = mortonIncY(morton);
    out[0] = mortonDecX(ym1);    // (-1, -1)
    out[1] = ym1;     // ( 0, -1)
    out[2] = mortonIncX(ym1);    // (+1, -1)
    out[3] = mortonDecX(morton); // (-1,  0)
    out[4] = morton;  // ( 0,  0)
    out[5] = mortonIncX(morton); // (+1,  0)
    out[6] = mortonDecX(yp1);    // (-1, +1)
    out[7] = yp1;     // ( 0, +1)
    out[8] = mortonIncX(yp1);    // (+1, +1)
}

// Returns true if n is a power of 2. Morton indexing requires this.
inline static bool isPow2(uint32_t n) { return n > 0 && (n & (n - 1)) == 0; }

using cell_idx = uint32_t;
using obj_idx = uint32_t;

struct SimpleSpatialGrid
{
    uint32_t CellsX = 0;
    uint32_t CellsY = 0;
    uint32_t cell_max_capacity = 0;

    float cell_width = 0;
    float cell_height = 0;
    float world_width = 0;
    float world_height = 0;

    alignas(64) std::vector<obj_idx>  grid{};
    alignas(64) std::vector<uint8_t>  cell_capacities{};

	// Used to calculate if particles have changed cells since last frames.
    std::vector<cell_idx> prev_cells;

public:
    explicit SimpleSpatialGrid(uint32_t cells_x, uint32_t cells_y, uint32_t cell_capacity,
        float world_width, float world_height)
        : CellsX(cells_x), CellsY(cells_y), cell_max_capacity(cell_capacity)
        , world_width(world_width), world_height(world_height)
    {
        // Morton indexing only works correctly with power-of-2 grid dimensions.
        // The Morton index of cell (cells_x-1, cells_y-1) may exceed cells_x*cells_y
        // for non-power-of-2 sizes, causing out-of-bounds access.
        assert(isPow2(cells_x) && "CellsX must be a power of 2 for Morton indexing");
        assert(isPow2(cells_y) && "CellsY must be a power of 2 for Morton indexing");

        // Total slots needed is not CellsX*CellsY but the maximum possible Morton
        // index + 1. For a grid of 2^a columns and 2^b rows the highest Morton
        // index is calcZOrder(CellsX-1, CellsY-1), so we size to that + 1.
        const uint32_t total = mortonTableSize(cells_x, cells_y);
        grid.resize(total * cell_max_capacity, 0);
        cell_capacities.resize(total, 0);

        update_cell_dimensions();
    }

    ~SimpleSpatialGrid() = default;

    void update_cell_dimensions()
    {
        cell_width = world_width / static_cast<float>(CellsX);
        cell_height = world_height / static_cast<float>(CellsY);
    }

    void change_cell_dimensions(uint32_t new_cells_x, uint32_t new_cells_y)
    {
        assert(isPow2(new_cells_x) && isPow2(new_cells_y));
        CellsX = new_cells_x;
        CellsY = new_cells_y;
        update_cell_dimensions();

        const uint32_t total = mortonTableSize(CellsX, CellsY);
        grid.assign(total * cell_max_capacity, 0);
        cell_capacities.assign(total, 0);
    }

    // --- hash is now Morton order instead of row-major ---
    cell_idx inline hash(const float x, const float y) const
    {
        const auto cell_x = static_cast<uint16_t>(x / cell_width);
        const auto cell_y = static_cast<uint16_t>(y / cell_height);
        return calcZOrder(cell_x, cell_y);
    }

    cell_idx inline add_object(const float x, const float y, const size_t obj_id)
    {
        const cell_idx index = hash(x, y);
        uint8_t& cap = cell_capacities[index];

        if (cap < cell_max_capacity)
            grid[index * cell_max_capacity + cap++] = static_cast<obj_idx>(obj_id);

        prev_cells[obj_id] = index;

        return index;
    }

    void clear()
    {
        std::memset(cell_capacities.data(), 0, cell_capacities.size());
    }

    size_t get_total_cells() const { return CellsX * CellsY; }

    void find(const float x, const float y, FixedSpan<obj_idx>* container) const
    {
        find_from_index(hash(x, y), container);
    }

    void find_from_index(const cell_idx index, FixedSpan<obj_idx>* container) const
    {
        container->count = 0;

        // Recover grid coordinates from Morton index for bounds checking
        const uint32_t cell_x = mortonToX(index);
        const uint32_t cell_y = mortonToY(index);

        uint32_t neighbours[9];
        mortonNeighbours3x3(index, neighbours);

        for (int i = 0; i < 9; ++i)
        {
            // Skip the neighbour if its decoded coordinates fall outside the grid.
            // This handles edge cells cleanly — mortonDecX on x=0 wraps to a huge
            // number, so the >= check catches it without any signed arithmetic.
            const uint32_t nx = mortonToX(neighbours[i]);
            const uint32_t ny = mortonToY(neighbours[i]);
            if (nx >= CellsX || ny >= CellsY) continue;

            const cell_idx  neighbour = neighbours[i];
            const uint8_t   count = cell_capacities[neighbour];

            const obj_idx* data = &grid[neighbour * cell_max_capacity];

            for (uint8_t j = 0; j < count; ++j)
                container->add(data[j]);
        }
    }

    void get_cell_range(float wx0, float wy0, float wx1, float wy1,
        uint32_t& cx0, uint32_t& cy0,
        uint32_t& cx1, uint32_t& cy1) const
    {
        cx0 = static_cast<uint32_t>(std::clamp(wx0 / cell_width, 0.f, static_cast<float>(CellsX - 1)));
        cy0 = static_cast<uint32_t>(std::clamp(wy0 / cell_height, 0.f, static_cast<float>(CellsY - 1)));
        cx1 = static_cast<uint32_t>(std::clamp(wx1 / cell_width, 0.f, static_cast<float>(CellsX - 1)));
        cy1 = static_cast<uint32_t>(std::clamp(wy1 / cell_height, 0.f, static_cast<float>(CellsY - 1)));
    }

    void track_stats(int& total, int& max_in, int& full, int& empty, int& inv)
    {
        // Iterate over logical cells only (not the full Morton table which has gaps)
        for (uint32_t cy = 0; cy < CellsY; ++cy)
        {
            for (uint32_t cx = 0; cx < CellsX; ++cx)
            {
                const cell_idx i = calcZOrder(static_cast<uint16_t>(cx),
                    static_cast<uint16_t>(cy));
                const int c = static_cast<int>(cell_capacities[i]);
                total += c;
                if (c == 0)                                    ++empty;
                if (c >= static_cast<int>(cell_max_capacity))  ++full;
                if (c > max_in)                                 max_in = c;
            }
        }

        const auto tc = static_cast<float>(get_total_cells());
        inv = tc > 0.f ? 1.f / tc : 0.f;

        ImGui::Spacing();
        ImGui::Text("Objects  %d", total);
        ImGui::Text("Avg/cell %.2f", tc > 0 ? static_cast<float>(total) * inv : 0.f);
        ImGui::Text("Max cell %d  (%.0f%%)", max_in,
            cell_max_capacity > 0 ? max_in * 100.f / static_cast<float>(cell_max_capacity) : 0.f);
        ImGui::Text("Full     %d  (%.1f%%)", full, full * 100.f * inv);
        ImGui::Text("Empty    %d  (%.1f%%)", empty, empty * 100.f * inv);

        if (full > 0)
            ImGui::TextColored({ 1.f, 0.4f, 0.4f, 1.f },
                "[!] %d at cap — objects may drop", full);
    }

private:
    // The Morton index of the last valid cell (CellsX-1, CellsY-1) is not
    // CellsX*CellsY-1. For power-of-2 dimensions the safe allocation size
    // is calcZOrder(CellsX-1, CellsY-1) + 1.
    static uint32_t mortonTableSize(uint32_t cx, uint32_t cy)
    {
        return calcZOrder(static_cast<uint16_t>(cx - 1),
            static_cast<uint16_t>(cy - 1)) + 1;
    }
};