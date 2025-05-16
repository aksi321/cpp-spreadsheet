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
    auto& row = cells_[pos.row];
    if (pos.col >= static_cast<int>(row.size())) {
        row.resize(pos.col + 1);
    }
}

Cell* Sheet::GetOrCreate(Position p)
{
    EnsureExists(p);
    if (!cells_[p.row][p.col])
        cells_[p.row][p.col] = make_unique<Cell>(*this);
    return cells_[p.row][p.col].get();
}

void Sheet::Link(Cell& from, Cell& to)
{
    from.children_.push_back(&to);
    to  .parents_ .push_back(&from);
}

void Sheet::UnlinkAll(Cell& node)
{
    for (Cell* ch : node.children_)
        ch->RemoveParent(&node);
    node.children_.clear();
}

void Sheet::RebuildDeps(Cell& node,
                        const std::vector<Position>& refs)
{
    for (const Position& p : refs)
        Link(node, *GetOrCreate(p));
}

bool Sheet::HasCircular(Cell& start,
                        Cell& cur,
                        std::unordered_set<Cell*>& vis) const
{
    if (&start == &cur)            return true;
    if (!vis.insert(&cur).second)  return false;

    for (Cell* ch : cur.children_)
        if (HasCircular(start, *ch, vis))
            return true;
    return false;
}


void Sheet::SetCell(Position pos, string text)
{
    CheckPosOrThrow(pos);

    if (text.empty()) { ClearCell(pos); return; }

    Cell* cell = GetOrCreate(pos);

    std::unique_ptr<Cell::Impl> tmp_impl;
    std::vector<Position>       new_refs;
    try {
        cell->Set(text);
        new_refs = cell->GetReferencedCells();
        tmp_impl = std::move(cell->impl_);
    } catch (const FormulaException&) {
        throw;
    }

    std::unordered_set<Cell*> vis;
    Cell  fake_holder(*this);
    fake_holder.children_.reserve(new_refs.size());
    for (const Position& p : new_refs)
        fake_holder.children_.push_back(GetOrCreate(p));

    if (HasCircular(*cell, fake_holder, vis))
        throw CircularDependencyException("circular");

    UnlinkAll(*cell);
    cell->impl_ = std::move(tmp_impl);
    cell->dirty_ = true;
    RebuildDeps(*cell, new_refs);
    cell->InvalidateCache();
}

const CellInterface* Sheet::GetCell(Position pos) const {
    CheckPosOrThrow(pos);

    if (pos.row < static_cast<int>(cells_.size())) {
        const auto& row = cells_[pos.row];
        if (pos.col < static_cast<int>(row.size())) {
            return row[pos.col].get();
        }
    }
    return nullptr;
}

CellInterface* Sheet::GetCell(Position pos) {
    CheckPosOrThrow(pos);

    if (pos.row < static_cast<int>(cells_.size())) {
        auto& row = cells_[pos.row];
        if (pos.col < static_cast<int>(row.size())) {
            return row[pos.col].get();
        }
    }
    return nullptr;
}

void Sheet::ClearCell(Position pos) {
    CheckPosOrThrow(pos);

    if (pos.row >= static_cast<int>(cells_.size())) return;
    auto& row = cells_[pos.row];
    if (pos.col >= static_cast<int>(row.size())) return;

    row[pos.col].reset();
    if (row[pos.col])
    row[pos.col]->InvalidateCache();
}

Size Sheet::GetPrintableSize() const {
    int max_row = 0;
    int max_col = 0;

    for (int r = 0; r < static_cast<int>(cells_.size()); ++r) {
        const auto& row = cells_[r];
        for (int c = 0; c < static_cast<int>(row.size()); ++c) {
            if (!CellIsEmpty(row[c])) {
                max_row = max(max_row, r + 1);
                max_col = max(max_col, c + 1);
            }
        }
    }
    return {max_row, max_col};
}

void Sheet::PrintValues(ostream& out) const {
    Size sz = GetPrintableSize();

    for (int r = 0; r < sz.rows; ++r) {
        for (int c = 0; c < sz.cols; ++c) {
            if (c) out.put('\t');

            const Cell* cell = nullptr;
            if (r < static_cast<int>(cells_.size()) &&
                c < static_cast<int>(cells_[r].size())) {
                cell = cells_[r][c].get();
            }

            if (cell && !cell->GetText().empty()) {
                PrintValue(cell->GetValue(), out);
            }
        }
        out.put('\n');
    }
}

void Sheet::PrintTexts(ostream& out) const {
    Size sz = GetPrintableSize();

    for (int r = 0; r < sz.rows; ++r) {
        for (int c = 0; c < sz.cols; ++c) {
            if (c) out.put('\t');

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
