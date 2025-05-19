#include "cell.h"
#include "sheet.h"

#include <algorithm>
#include <cctype>
#include <ostream>
#include <sstream>
#include <utility>
#include <variant>

using namespace std;

namespace {

bool IsFormula(const string& text) {
    return text.size() > 1 && text.front() == FORMULA_SIGN &&
           text.find_first_not_of(' ', 1) != string::npos;
}

bool StartsWithEscape(const string& text) {
    return !text.empty() && text.front() == ESCAPE_SIGN;
}

} // namespace

class Cell::EmptyImpl final : public Cell::Impl {
public:
    Value GetValue(const SheetInterface&) const override {
         return string{};
    }
    string GetText() const override {
     return string{};
    }
};

class Cell::TextImpl final : public Cell::Impl {
public:
    explicit TextImpl(string text)
        : raw_(std::move(text))
        , visible_(StartsWithEscape(raw_) ? raw_.substr(1) : raw_) {}

    Value GetValue(const SheetInterface&) const override {
        return visible_;
    }
    string GetText() const override {
        return raw_;
    }

private:
    string raw_;
    string visible_;
};

class Cell::FormulaImpl final : public Cell::Impl {
public:
    FormulaImpl(string raw, Sheet& sheet) {
        size_t p = 1;
        while (p < raw.size() && isspace(static_cast<unsigned char>(raw[p]))) {
            ++p;
        }
        formula_ = ParseFormula(raw.substr(p));
        text_    = FORMULA_SIGN + formula_->GetExpression();
        refs_    = formula_->GetReferencedCells();
    }

    Value GetValue(const SheetInterface& sheet) const override {
        auto v = formula_->Evaluate(sheet);
        if (auto num = std::get_if<double>(&v)) {
            return Value(*num);
        }
        return Value(std::get<FormulaError>(v));
    }

    string GetText() const override {
        return text_;
    }
    vector<Position> GetRefs() const override { 
        return refs_; 
    }

private:
    unique_ptr<FormulaInterface> formula_;
    string text_;
    vector<Position> refs_;
};

Cell::Cell(Sheet& sheet)
    : sheet_(sheet)
    , impl_(make_unique<EmptyImpl>()) {}

Cell::~Cell() = default;

void Cell::Set(string text) {
    unique_ptr<Impl> new_impl;

    if (text.empty()) {
        new_impl = make_unique<EmptyImpl>();
    } else if (!StartsWithEscape(text) && IsFormula(text)) {
        new_impl = make_unique<FormulaImpl>(text, sheet_);
    } else {
        new_impl = make_unique<TextImpl>(text);
    }

    vector<Position> refs = new_impl->GetRefs();

    sheet_.dfs_visited_.clear(); 
    Cell fake_holder(sheet_);
    fake_holder.children_.reserve(refs.size());
    for (const Position& p : refs) {
        fake_holder.children_.push_back(sheet_.GetOrCreate(p));
    }
    if (sheet_.HasCircular(*this, fake_holder)) {
        throw CircularDependencyException("Circular dependency");
    }

AdoptImpl(std::move(new_impl), refs);

}

void Cell::Clear() {
    Set("");
}

CellInterface::Value Cell::GetValue() const {
    if (!dirty_) {
        return *cache_;
    }
    cache_ = impl_->GetValue(sheet_);
    dirty_ = false;
    return *cache_;
}

string Cell::GetText() const {
    return impl_->GetText();
}

vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetRefs();
}

void Cell::RemoveParent(Cell* p) {
    parents_.erase(remove(parents_.begin(), parents_.end(), p), parents_.end());
}

void Cell::InvalidateCache() {
    if (dirty_) {
        return;
    }
    dirty_ = true;
    for (Cell* parent : parents_) {
        parent->InvalidateCache();
    }
}

void Cell::AdoptImpl(unique_ptr<Impl> new_impl,
                     const vector<Position>& refs) {

    for (Cell* child : children_) {
        child->RemoveParent(this);
    }
    
    children_.clear();

    for (const Position& pos : refs) {
        Cell* child = sheet_.GetOrCreate(pos);
        children_.push_back(child);
        child->parents_.push_back(this);
    }

    impl_ = std::move(new_impl);
    InvalidateCache();
}

bool Cell::IsReferenced() const {
    return !parents_.empty();
}
