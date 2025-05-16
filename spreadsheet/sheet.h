#pragma once

#include "cell.h"
#include "common.h"

#include <vector>
#include <memory>
#include <ostream>
#include <unordered_set>

class Sheet : public SheetInterface {
public:
    ~Sheet() override = default;

    void               SetCell  (Position pos, std::string text) override;
    const CellInterface* GetCell(Position pos) const override;
    CellInterface*       GetCell(Position pos)       override;
    void               ClearCell(Position pos)       override;
    Size               GetPrintableSize()            const override;
    void               PrintValues(std::ostream& out)const override;
    void               PrintTexts (std::ostream& out)const override;

    friend class Cell;
private:
    using Row = std::vector<std::unique_ptr<Cell>>;
    std::vector<Row> cells_;

    static void CheckPosOrThrow(const Position& p);
    void        EnsureExists (const Position& p);

    Cell*       GetOrCreate  (Position p);

    void        Link    (Cell& from, Cell& to);
    void        UnlinkAll(Cell& node);
    void        RebuildDeps(Cell& node,
                            const std::vector<Position>& refs);
    bool        HasCircular(Cell& start,
                           Cell& cur,
                           std::unordered_set<Cell*>& visited) const;

};
