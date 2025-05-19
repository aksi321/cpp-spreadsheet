#pragma once

#include "common.h"
#include "formula.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

class Sheet;

class Cell final : public CellInterface {
public:
    explicit Cell(Sheet& owner);
    ~Cell();

    void Set (std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText () const override;
    std::vector<Position> GetReferencedCells() const override;

    const std::vector<Cell*>& Children() const noexcept { return children_; }
    void AddParent (Cell* p) { parents_.push_back(p); }
    void RemoveParent(Cell* p);
    void InvalidateCache();
    bool IsReferenced() const;

private:
    class Impl {
    public:
        virtual ~Impl() = default;

        virtual Value GetValue(const SheetInterface&) const = 0;
        virtual std::string GetText() const = 0;
        virtual std::vector<Position> GetRefs() const { return {}; }
    };

    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    Sheet& sheet_;
    std::unique_ptr<Impl> impl_;

    mutable std::optional<Value> cache_;
    mutable bool dirty_ = true;

    std::vector<Cell*> parents_;
    std::vector<Cell*> children_;

    void AdoptImpl(std::unique_ptr<Impl> new_impl,
                   const std::vector<Position>& refs);

    friend class Sheet;
};
