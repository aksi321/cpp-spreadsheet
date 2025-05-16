#include "formula.h"
#include "FormulaAST.h"
#include "common.h"

#include <sstream>
#include <utility>

using namespace std;

namespace {

class Formula : public FormulaInterface {
public:
    explicit Formula(string expression) try
        : ast_(ParseFormulaAST(expression))
    {
    } catch (const exception& e) {
        throw FormulaException(e.what());
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        try {
            return ast_.Execute(sheet);
        } catch (const FormulaError& fe) {
            return fe;
        }
    }

    string GetExpression() const override {
        ostringstream oss;
        ast_.PrintFormula(oss);
        return oss.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        const auto& fl = ast_.GetCells();
        return {fl.begin(), fl.end()};
    }

private:
    FormulaAST ast_;
};

}  // namespace

unique_ptr<FormulaInterface> ParseFormula(string expression) {
    return make_unique<Formula>(move(expression));
}
