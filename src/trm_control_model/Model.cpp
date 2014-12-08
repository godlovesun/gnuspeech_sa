/***************************************************************************
 *  Copyright 2014 Marcelo Y. Matuda                                       *
 *  Copyright 1991, 1992, 1993, 1994, 1995, 1996, 2001, 2002               *
 *    David R. Hill, Leonard Manzara, Craig Schock                         *
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
// 2014-09
// This file was created by Marcelo Y. Matuda, and code/information
// from Gnuspeech was added to it later.

#include "Model.h"

#include <iostream>
#include <utility> /* make_pair */

#include "XMLConfigFile.h"
#include "Log.h"



namespace GS {
namespace TRMControlModel {

/*******************************************************************************
 * Constructor.
 */
Model::Model()
{
}

/*******************************************************************************
 * Destructor.
 */
Model::~Model()
{
}

/*******************************************************************************
 *
 */
void
Model::load(const char* configDirPath, const char* configFileName)
{
	std::string filePath = std::string(configDirPath) + configFileName;

	// Load the configuration file.
	LOG_DEBUG("Loading xml configuration: " << filePath);
	XMLConfigFile cfg(*this, filePath);
	cfg.loadModel();

	prepareCategories();
	preparePostures();
	prepareEquations();
	prepareRules();
}

/*******************************************************************************
 *
 */
void
Model::prepareCategories()
{
	LOG_DEBUG("Preparing categories...");

	categoryMap_.clear();

	for (auto& category : categoryList_) {
		auto res = categoryMap_.insert(std::make_pair(category.name, &category));
		if (!res.second) {
			THROW_EXCEPTION(TRMControlModelException, "Duplicate category: " << category.name << '.');
		}
	}
}

/*******************************************************************************
 *
 */
void
Model::preparePostures()
{
	LOG_DEBUG("Preparing postures...");

	// Fill the category code in the category list of each Posture.
	for (auto& posturePair : postureMap_) {
		for (auto& postureCategory : posturePair.second->categoryList()) {
			auto catIter = categoryMap_.find(postureCategory.name);
			if (catIter != categoryMap_.end()) {
				postureCategory.code = catIter->second->code;
			}
		}
	}
}

/*******************************************************************************
 *
 */
void
Model::prepareEquations()
{
	LOG_DEBUG("Preparing equations...");

	// Convert the formula expression to a tree.
	for (EquationMap::const_iterator iter = equationMap_.begin(); iter != equationMap_.end(); ++iter) {
		Equation& e = *iter->second;
		e.parseFormula(formulaSymbol_);
	}
}

/*******************************************************************************
 *
 */
void
Model::prepareRules()
{
	LOG_DEBUG("Preparing rules...");

	// Convert the boolean expression string of each Rule to a tree.
	for (RuleList::const_iterator iter = ruleList_.begin(); iter != ruleList_.end(); ++iter) {
		Rule& r = **iter;
		r.parseBooleanExpression(categoryMap_);
	}
}

/*******************************************************************************
 *
 */
void
Model::printInfo() const
{
	//---------------------------------------------------------
	// Categories.
	std::cout << std::string(40, '-') << "\nCategories:\n" << std::endl;
	for (auto iter = categoryMap_.begin();
			iter != categoryMap_.end(); ++iter) {
		std::cout << "category: " << iter->first << " code: " << iter->second->code << std::endl;
	}

	//---------------------------------------------------------
	// Postures.
	std::cout << std::string(40, '-') << "\nPostures:\n" << std::endl;
	for (PostureMap::const_iterator iter = postureMap_.begin(); iter != postureMap_.end(); ++iter) {
		const PostureMap::value_type& v = *iter;
		std::cout << "posture symbol: " << v.second->name() << " r8: " << v.second->getParameterTarget(14 /* R8 */) << std::endl;

		for (auto it2 = v.second->categoryList().begin();
				it2 != v.second->categoryList().end(); ++it2) {
			std::cout << "  categ: " << it2->name << "_" << it2->code << std::endl;
		}
		std::cout << "  symbols.duration: "   << v.second->symbols().duration   << std::endl;
		std::cout << "  symbols.transition: " << v.second->symbols().transition << std::endl;
		std::cout << "  symbols.qssa: "       << v.second->symbols().qssa       << std::endl;
		std::cout << "  symbols.qssb: "       << v.second->symbols().qssb       << std::endl;
	}

	//---------------------------------------------------------
	// Equations.
	std::cout << std::string(40, '-') << "\nEquations:\n" << std::endl;
	FormulaSymbolList symbolList;
	symbolList[FormulaSymbol::SYMB_TRANSITION1] = 100.1f;
	symbolList[FormulaSymbol::SYMB_TRANSITION2] = 100.2f;
	symbolList[FormulaSymbol::SYMB_TRANSITION3] = 100.3f;
	symbolList[FormulaSymbol::SYMB_TRANSITION4] = 100.4f;
	symbolList[FormulaSymbol::SYMB_QSSA1] = 20.5f;
	symbolList[FormulaSymbol::SYMB_QSSA2] = 20.6f;
	symbolList[FormulaSymbol::SYMB_QSSA3] = 20.7f;
	symbolList[FormulaSymbol::SYMB_QSSA4] = 20.8f;
	symbolList[FormulaSymbol::SYMB_QSSB1] = 20.9f;
	symbolList[FormulaSymbol::SYMB_QSSB2] = 20.0f;
	symbolList[FormulaSymbol::SYMB_QSSB3] = 20.1f;
	symbolList[FormulaSymbol::SYMB_QSSB4] = 20.2f;
	symbolList[FormulaSymbol::SYMB_TEMPO1] = 1.1f;
	symbolList[FormulaSymbol::SYMB_TEMPO2] = 1.2f;
	symbolList[FormulaSymbol::SYMB_TEMPO3] = 1.3f;
	symbolList[FormulaSymbol::SYMB_TEMPO4] = 1.4f;
	symbolList[FormulaSymbol::SYMB_RD] = 150.4f;
	symbolList[FormulaSymbol::SYMB_MARK1] = 150.5f;
	symbolList[FormulaSymbol::SYMB_MARK2] = 150.6f;
	//symbolList[FormulaSymbol::SYMB_NULL] = 1.0f;
	for (EquationMap::const_iterator iter = equationMap_.begin(); iter != equationMap_.end(); ++iter) {
		const EquationMap::value_type& v = *iter;
		std::cout << "=== Equation: [" << v.second->name << "] group: " << v.second->groupName << std::endl;
		std::cout << "    [" << v.second->formula << "]" << std::endl;
		std::cout << *v.second->formulaRoot << std::endl;
		std::cout << "*** EVAL=" << v.second->formulaRoot->eval(symbolList) << std::endl;
	}

	//---------------------------------------------------------
	// Transitions.
	std::cout << std::string(40, '-') << "\nTransitions:" << std::endl << std::endl;
	for (auto& item : transitionMap_) {
		const Transition& t = *item.second;
		std::cout << "### Transition: [" << t.name() << "] group: " << t.groupName()  << std::endl;
		std::cout << "    type=" << t.type() << " special=" << t.special() << std::endl;

		for (auto& pointOrSlope : t.pointOrSlopeList()) {
			if (!pointOrSlope->isSlopeRatio()) {
				const auto& point = dynamic_cast<const Transition::Point&>(*pointOrSlope);
				std::cout << "       point: type=" << point.type << " value=" << point.value
					<< " freeTime=" << point.freeTime << " timeExpression=" << point.timeExpression
					<< " isPhantom=" << point.isPhantom << std::endl;
			} else {
				const auto& slopeRatio = dynamic_cast<const Transition::SlopeRatio&>(*pointOrSlope);
				for (auto& point : slopeRatio.pointList) {
					std::cout << "         point: type=" << point->type << " value=" << point->value
						<< " freeTime=" << point->freeTime << " timeExpression=" << point->timeExpression
						<< " isPhantom=" << point->isPhantom << std::endl;
				}
				for (auto& slope : slopeRatio.slopeList) {
					std::cout << "         slope: slope=" << slope->slope << " displayTime=" << slope->displayTime << std::endl;
				}
			}
		}
	}

	//---------------------------------------------------------
	// Special transitions.
	std::cout << std::string(40, '-') << "\nSpecial transitions:" << std::endl << std::endl;
	for (auto& item : specialTransitionMap_) {
		const Transition& t = *item.second;
		std::cout << "### Transition: [" << t.name() << "] group: " << t.groupName()  << std::endl;
		std::cout << "    type=" << t.type() << " special=" << t.special() << std::endl;

		for (const auto& pointOrSlope : t.pointOrSlopeList()) {
			const auto& point = dynamic_cast<const Transition::Point&>(*pointOrSlope);

			std::cout << "       point: type=" << point.type << " value=" << point.value
				<< " freeTime=" << point.freeTime << " timeExpression=" << point.timeExpression
				<< " isPhantom=" << point.isPhantom << std::endl;
		}
	}

	//---------------------------------------------------------
	// Rules.

	// Print boolean expression tree for each Rule.
	std::cout << std::string(40, '-') << "\nRules:\n" << std::endl;
	for (RuleList::const_iterator iter = ruleList_.begin(); iter != ruleList_.end(); ++iter) {
		const Rule& r = **iter;
		std::cout << "--------------------------------------" << std::endl;
		std::cout << "Rule number: " << r.number() << '\n' << std::endl;
		r.printBooleanNodeTree();
	}
	std::cout << "--------------------------------------" << std::endl;
	PostureSequence postSeq;
	const Posture* pp = findPosture("m");
	if (pp) postSeq.push_back(pp);
	pp = findPosture("ah");
	if (pp) postSeq.push_back(pp);
	pp = findPosture("s");
	if (pp) postSeq.push_back(pp);
	for (RuleList::const_iterator iter = ruleList_.begin(); iter != ruleList_.end(); ++iter) {
		const Rule& r = **iter;
		std::cout << "---" << std::endl;
		std::cout << "Rule number: " << r.number() << std::endl;
		std::cout << "bool=" << r.evalBooleanExpression(postSeq) << std::endl;
	}
	std::cout << "--------------------------------------" << std::endl;
	for (RuleList::const_iterator iter = ruleList_.begin(); iter != ruleList_.end(); ++iter) {
		const Rule& r = **iter;
		std::cout << "---" << std::endl;
		std::cout << "Rule number: " << r.number() << std::endl;
		std::cout << "Number of boolean expressions = " << r.numberOfExpressions() << std::endl;
	}
}

/*******************************************************************************
 *
 */
unsigned int
Model::findParameterIndex(const std::string& name) const
{
	for (unsigned int i = 0; i < parameterList_.size(); ++i) {
		if (parameterList_[i]->name() == name) {
			return i;
		}
	}
	THROW_EXCEPTION(InvalidParameterException, "Parameter name not found: " << name << '.');
}

/*******************************************************************************
 * Find the Category code.
 *
 * Returns nullptr if the Category was not found.
 */
const Category*
Model::findCategory(const std::string& name) const
{
	auto iter = categoryMap_.find(name);
	if (iter != categoryMap_.end()) {
		return iter->second;
	}
	return nullptr;
}

/*******************************************************************************
 *
 */
unsigned int
Model::getCategoryCode(const std::string& name) const
{
	auto iter = categoryMap_.find(name);
	if (iter == categoryMap_.end()) {
		THROW_EXCEPTION(TRMControlModelException, "Category not found: " << name << '.');
	}
	return iter->second->code;
}

} /* namespace TRMControlModel */
} /* namespace GS */
