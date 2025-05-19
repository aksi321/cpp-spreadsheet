#include "sheet.h"

#include <algorithm>
#include <sstream>
#include <variant>

using namespace std;

namespace {

bool CellIsEmpty(const unique_ptr<Cell>& ptr) {
    return !ptr || ptr->GetText().empty();
}

void PrintValue(const CellInterface::Value& v, ostream& out) {
    visit([&out](const auto& x) { out << x; }, v);
}

}  // namespace

void Sheet::CheckPosOrThrow(const Position& pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }
}

void Sheet::EnsureExists(const Position& pos) {
    if (pos.row >= static_cast<int>(cells_.size())) {
        cells_.resize(pos.row + 1);
    }
    Row& row = cells_[pos.row];
    if (pos.col >= static_cast<int>(row.size())) {
        row.resize(pos.col + 1);
    }
}

Cell* Sheet::GetOrCreate(Position p) {
    EnsureExists(p);
    if (!cells_[p.row][p.col]) {
        cells_[p.row][p.col] = make_unique<Cell>(*this);
    }
    return cells_[p.row][p.col].get();
}

void Sheet::Link(Cell& from, Cell& to) {
    from.children_.push_back(&to);
    to  .parents_ .push_back(&from);
}

void Sheet::UnlinkAll(Cell& node) {
    for (Cell* child : node.children_) {
        child->RemoveParent(&node);
    }
    node.children_.clear();
}

void Sheet::RebuildDeps(Cell& node, const vector<Position>& refs) {
    for (const Position& p : refs) {
        Link(node, *GetOrCreate(p));
    }
}

bool Sheet::HasCircular(const Cell& start, const Cell& cur) const {
    if (&start == &cur) {
        return true;
    }
    if (!dfs_visited_.insert(&cur).second) {
        return false;
    }
    for (Cell* child : cur.children_) {
        if (HasCircular(start, *child)) {
            return true;
        }
    }
    return false;
}

void Sheet::SetCell(Position pos, std::string text) {
    CheckPosOrThrow(pos);

    if (text.empty()) {
        ClearCell(pos);
        return;
    }

    GetOrCreate(pos)->Set(std::move(text));
}

const CellInterface* Sheet::GetCell(Position pos) const {
    CheckPosOrThrow(pos);

    if (pos.row < static_cast<int>(cells_.size())) {
        const Row& row = cells_[pos.row];
        if (pos.col < static_cast<int>(row.size())) {
            return row[pos.col].get();
        }
    }
    return nullptr;
}

CellInterface* Sheet::GetCell(Position pos) {
    CheckPosOrThrow(pos);

    if (pos.row < static_cast<int>(cells_.size())) {
        Row& row = cells_[pos.row];
        if (pos.col < static_cast<int>(row.size())) {
            return row[pos.col].get();
        }
    }
    return nullptr;
}

void Sheet::ClearCell(Position pos) {
    CheckPosOrThrow(pos);

    if (pos.row >= static_cast<int>(cells_.size())) {
        return;
    }
    auto& row = cells_[pos.row];
    if (pos.col >= static_cast<int>(row.size())) {
        return;
    }

    if (row[pos.col]) {
        row[pos.col]->InvalidateCache();
    }
    row[pos.col].reset();
}

Size Sheet::GetPrintableSize() const {
    int max_row = 0;
    int max_col = 0;

    for (int row_idx = 0; row_idx < static_cast<int>(cells_.size()); ++row_idx) {
        const auto& current_row = cells_[row_idx];
        for (int col_idx = 0; col_idx < static_cast<int>(current_row.size()); ++col_idx) {
            if (!CellIsEmpty(current_row[col_idx])) {
                max_row = max(max_row, row_idx + 1);
                max_col = max(max_col, col_idx + 1);
            }
        }
    }
    return {max_row, max_col};
}

void Sheet::PrintValues(ostream& out) const {
    Size sz = GetPrintableSize();

    for (int row_idx = 0; row_idx < sz.rows; ++row_idx) {
        for (int col_idx = 0; col_idx < sz.cols; ++col_idx) {
            if (col_idx) {
                out.put('\t');
            }

            const Cell* cell_ptr = nullptr;
            if (row_idx < static_cast<int>(cells_.size()) &&
                col_idx < static_cast<int>(cells_[row_idx].size())) {
                cell_ptr = cells_[row_idx][col_idx].get();
            }

            if (cell_ptr && !cell_ptr->GetText().empty()) {
                PrintValue(cell_ptr->GetValue(), out);
            }
        }
        out.put('\n');
    }
}

void Sheet::PrintTexts(ostream& out) const {
    Size sz = GetPrintableSize();

    for (int r = 0; r < sz.rows; ++r) {
        for (int c = 0; c < sz.cols; ++c) {
            if (c) {
                out.put('\t');
            }

            const Cell* cell = nullptr;
            if (r < static_cast<int>(cells_.size()) &&
                c < static_cast<int>(cells_[r].size())) {
                cell = cells_[r][c].get();
            }

            if (cell && !cell->GetText().empty()) {
                out << cell->GetText();
            }
        }
        out.put('\n');
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return make_unique<Sheet>();
}
