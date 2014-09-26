/***************************************************************************
 *  Copyright 2014 Marcelo Y. Matuda                                       *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "Equation.h"

#include <iostream>
#include <cassert>
#include <string>

#include "Exception.h"
#include "Log.h"
#include "Text.h"



namespace {

using namespace TRMControlModel;

const char ADD_CHAR         = '+';
const char SUB_CHAR         = '-';
const char MULT_CHAR        = '*';
const char DIV_CHAR         = '/';
const char RIGHT_PAREN_CHAR = ')';
const char LEFT_PAREN_CHAR  = '(';



class FormulaNodeParser {
public:
	FormulaNodeParser(const std::string& s, const FormulaSymbol& formulaSymbol)
				: formulaSymbolMap_(formulaSymbol.codeMap)
				, s_(s)
				, pos_(0)
				, symbolType_(SYMBOL_TYPE_INVALID) {
		if (s.empty()) {
			THROW_EXCEPTION(TRMControlModelException, "Formula expression parser error: Empty string.");
		}
		nextSymbol();
	}

	void throwException(const char* errorDescription) const;
	template<typename T> void throwException(const char* errorDescription, const T& complement) const;
	bool finished() const {
		return pos_ >= s_.size();
	}
	void nextSymbol();
	FormulaNode_ptr parseFactor();
	FormulaNode_ptr parseTerm();
	FormulaNode_ptr parseExpression();
private:
	enum SymbolType {
		SYMBOL_TYPE_INVALID,
		SYMBOL_TYPE_ADD,
		SYMBOL_TYPE_SUB,
		SYMBOL_TYPE_MULT,
		SYMBOL_TYPE_DIV,
		SYMBOL_TYPE_RIGHT_PAREN,
		SYMBOL_TYPE_LEFT_PAREN,
		SYMBOL_TYPE_STRING
	};

	const FormulaSymbol::CodeMap& formulaSymbolMap_;
	const std::string s_;
	std::string::size_type pos_;
	std::string symbol_;
	SymbolType symbolType_;

	void skipSpaces();

	static bool isSeparator(char c) {
		switch (c) {
		case RIGHT_PAREN_CHAR: return true;
		case LEFT_PAREN_CHAR:  return true;
		case ' ':              return true;
		default:               return false;
		}
	}
};

void
FormulaNodeParser::throwException(const char* errorDescription) const
{
	THROW_EXCEPTION(TRMControlModelException, "Formula expression parser error: " << errorDescription
					<< " at position " << pos_ << " of string [" << s_ << "].");
}

template<typename T>
void
FormulaNodeParser::throwException(const char* errorDescription, const T& complement) const
{
	THROW_EXCEPTION(TRMControlModelException, "Formula expression parser error: " << errorDescription << complement
					<< " at position " << pos_ << " of string [" << s_ << "].");
}

void
FormulaNodeParser::skipSpaces()
{
	while (!finished() && s_[pos_] == ' ') ++pos_;
}

void
FormulaNodeParser::nextSymbol()
{
	skipSpaces();

	symbol_.clear();

	if (finished()) {
		symbolType_ = SYMBOL_TYPE_INVALID;
		return;
	}

	char c = s_[pos_++];
	switch (c) {
	case ADD_CHAR:
		symbolType_ = SYMBOL_TYPE_ADD;
		break;
	case SUB_CHAR:
		symbolType_ = SYMBOL_TYPE_SUB;
		break;
	case MULT_CHAR:
		symbolType_ = SYMBOL_TYPE_MULT;
		break;
	case DIV_CHAR:
		symbolType_ = SYMBOL_TYPE_DIV;
		break;
	case RIGHT_PAREN_CHAR:
		symbolType_ = SYMBOL_TYPE_RIGHT_PAREN;
		break;
	case LEFT_PAREN_CHAR:
		symbolType_ = SYMBOL_TYPE_LEFT_PAREN;
		break;
	default:
		symbol_ += c;
		symbolType_ = SYMBOL_TYPE_STRING;
		while ( !finished() && !isSeparator(c = s_[pos_]) ) {
			symbol_ += c;
			++pos_;
		}
	}
}

/*******************************************************************************
 * FACTOR -> "(" EXPRESSION ")" | SYMBOL | CONST | ADD_OP FACTOR
 */
FormulaNode_ptr
FormulaNodeParser::parseFactor()
{
	switch (symbolType_) {
	case SYMBOL_TYPE_LEFT_PAREN: // (expression)
	{
		nextSymbol();
		FormulaNode_ptr res(parseExpression());
		if (symbolType_ != SYMBOL_TYPE_RIGHT_PAREN) {
			throwException("Right parenthesis not found");
		}
		nextSymbol();
		return std::move(res);
	}
	case SYMBOL_TYPE_ADD: // unary plus
		nextSymbol();
		return parseFactor();
	case SYMBOL_TYPE_SUB: // unary minus
		nextSymbol();
		return FormulaNode_ptr(new FormulaMinusUnaryOp(parseFactor()));
	case SYMBOL_TYPE_STRING: // const / symbol
	{
		std::string symbolTmp = symbol_;
		nextSymbol();
		FormulaSymbol::CodeMap::const_iterator iter = formulaSymbolMap_.find(symbolTmp);
		if (iter == formulaSymbolMap_.end()) {
			// It's not a symbol.
			return FormulaNode_ptr(new FormulaConst(Text::parseString<float>(symbolTmp)));
		} else {
			return FormulaNode_ptr(new FormulaSymbolValue(iter->second));
		}
	}
	case SYMBOL_TYPE_RIGHT_PAREN:
		throwException("Unexpected symbol: ", RIGHT_PAREN_CHAR);
		break; // unreachable
	case SYMBOL_TYPE_MULT:
		throwException("Unexpected symbol: ", MULT_CHAR);
		break; // unreachable
	case SYMBOL_TYPE_DIV:
		throwException("Unexpected symbol: ", DIV_CHAR);
		break; // unreachable
	default:
		throwException("Invalid symbol");
		break; // unreachable
	}
	return FormulaNode_ptr(); // unreachable
}

/*******************************************************************************
 * TERM -> FACTOR { MULT_OP FACTOR }
 */
FormulaNode_ptr FormulaNodeParser::parseTerm()
{
	FormulaNode_ptr term1 = parseFactor();

	SymbolType type = symbolType_;
	while (type == SYMBOL_TYPE_MULT || type == SYMBOL_TYPE_DIV) {
		nextSymbol();
		FormulaNode_ptr term2 = parseFactor();
		FormulaNode_ptr expr;
		if (type == SYMBOL_TYPE_MULT) {
			expr.reset(new FormulaMultBinaryOp(std::move(term1), std::move(term2)));
		} else {
			expr.reset(new FormulaDivBinaryOp(std::move(term1), std::move(term2)));
		}
		term1.swap(expr);
		type = symbolType_;
	}

	return term1;
}

/*******************************************************************************
 * EXPRESSION -> TERM { ADD_OP TERM }
 */
FormulaNode_ptr
FormulaNodeParser::parseExpression()
{
	FormulaNode_ptr term1 = parseTerm();

	SymbolType type = symbolType_;
	while (type == SYMBOL_TYPE_ADD || type == SYMBOL_TYPE_SUB) {

		nextSymbol();
		FormulaNode_ptr term2 = parseTerm();

		FormulaNode_ptr expr;
		if (type == SYMBOL_TYPE_ADD) {
			expr.reset(new FormulaAddBinaryOp(std::move(term1), std::move(term2)));
		} else {
			expr.reset(new FormulaSubBinaryOp(std::move(term1), std::move(term2)));
		}

		term1 = std::move(expr);
		type = symbolType_;
	}

	return term1;
}

} /* namespace */

//==============================================================================

namespace TRMControlModel {

float
FormulaMinusUnaryOp::eval(const FormulaSymbolList& symbolList) const
{
	return -(child_->eval(symbolList));
}

void
FormulaMinusUnaryOp::print(std::ostream& out, int level) const
{
	std::string prefix(level * 8, ' ');
	out << prefix << "- [\n";

	child_->print(out, level + 1);

	out << prefix << "]" << std::endl;
}

float
FormulaAddBinaryOp::eval(const FormulaSymbolList& symbolList) const
{
	return child1_->eval(symbolList) + child2_->eval(symbolList);
}

void
FormulaAddBinaryOp::print(std::ostream& out, int level) const
{
	std::string prefix(level * 8, ' ');
	out << prefix << "+ [\n";

	child1_->print(out, level + 1);
	child2_->print(out, level + 1);

	out << prefix << "]" << std::endl;
}

float
FormulaSubBinaryOp::eval(const FormulaSymbolList& symbolList) const
{
	return child1_->eval(symbolList) - child2_->eval(symbolList);
}

void
FormulaSubBinaryOp::print(std::ostream& out, int level) const
{
	std::string prefix(level * 8, ' ');
	out << prefix << "- [\n";

	child1_->print(out, level + 1);
	child2_->print(out, level + 1);

	out << prefix << "]" << std::endl;
}

float
FormulaMultBinaryOp::eval(const FormulaSymbolList& symbolList) const
{
	return child1_->eval(symbolList) * child2_->eval(symbolList);
}

void
FormulaMultBinaryOp::print(std::ostream& out, int level) const
{
	std::string prefix(level * 8, ' ');
	out << prefix << "* [\n";

	child1_->print(out, level + 1);
	child2_->print(out, level + 1);

	out << prefix << "]" << std::endl;
}

float
FormulaDivBinaryOp::eval(const FormulaSymbolList& symbolList) const
{
	return child1_->eval(symbolList) / child2_->eval(symbolList);
}

void
FormulaDivBinaryOp::print(std::ostream& out, int level) const
{
	std::string prefix(level * 8, ' ');
	out << prefix << "/ [\n";

	child1_->print(out, level + 1);
	child2_->print(out, level + 1);

	out << prefix << "]" << std::endl;
}

float
FormulaConst::eval(const FormulaSymbolList& /*symbolList*/) const
{
	return value_;
}

void
FormulaConst::print(std::ostream& out, int level) const
{
	out << std::string(level * 8, ' ') << "const=" << value_ << std::endl;
}

float
FormulaSymbolValue::eval(const FormulaSymbolList& symbolList) const
{
	return symbolList[symbol_];
}

void
FormulaSymbolValue::print(std::ostream& out, int level) const
{
	out << std::string(level * 8, ' ') << "symbol=" << symbol_ << std::endl;
}

std::ostream&
operator<<(std::ostream& out, const FormulaNode& node)
{
	node.print(out);
	return out;
}

/*******************************************************************************
 *
 */
void
Equation::parseFormula(const FormulaSymbol& formulaSymbol)
{
	if (formula.empty()) {
		formulaRoot.reset(nullptr);
	} else {
		FormulaNodeParser p(formula, formulaSymbol);
		formulaRoot = p.parseExpression();
	}
}

/*******************************************************************************
 *
 */
float
Equation::evalFormula(const FormulaSymbolList& symbolList) const
{
	//if (formulaRoot.get() == 0) throw ControllerException("Equation::evalFormula: Formula tree not found.");
	assert(formulaRoot.get() != nullptr);

	return formulaRoot->eval(symbolList);
}

} /* namespace TRMControlModel */
