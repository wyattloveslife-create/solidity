/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

/// Compares two Yul object trees structurally, treating variable and  user-defined function names as equivalent
/// if they correspond 1:1 (tracked via a scoped bidirectional map). Prints a diff at the first point of divergence.

#include <libyul/AST.h>
#include <libyul/AsmPrinter.h>
#include <libyul/Object.h>
#include <libyul/ObjectParser.h>
#include <libyul/Dialect.h>
#include <libyul/backends/evm/EVMDialect.h>

#include <liblangutil/CharStream.h>
#include <liblangutil/DebugInfoSelection.h>
#include <liblangutil/ErrorReporter.h>
#include <liblangutil/EVMVersion.h>
#include <liblangutil/Scanner.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <map>
#include <vector>
#include <variant>


using namespace solidity;
using namespace solidity::yul;
using namespace solidity::langutil;

static std::string readFile(std::string_view _path)
{
	std::filesystem::path const file(_path);
	if (!std::filesystem::exists(file) || !std::filesystem::is_regular_file(file))
		throw std::runtime_error("File does not exist: " + std::string(_path));
	std::ifstream is(file, std::ios::binary | std::ios::ate);
	if (!is)
		throw std::runtime_error("Failed to open file: " + std::string(_path));
	auto size = is.tellg();
	std::string result(static_cast<std::size_t>(size), '\0');
	is.seekg(0);
	if (!is.read(result.data(), size))
		throw std::runtime_error("Failed to read file: " + std::string(_path));
	return result;
}

static std::shared_ptr<Object> parseYulFile(std::string_view const _path)
{
	std::string source = readFile(_path);
	Dialect const& dialect = EVMDialect::strictAssemblyForEVMObjects(EVMVersion::current(), std::nullopt);
	ErrorList errors;
	ErrorReporter errorReporter(errors);
	auto const charStream = std::make_shared<CharStream>(source, std::string(_path));
	auto const scanner = std::make_shared<Scanner>(*charStream);
	auto object = ObjectParser(errorReporter, dialect).parse(scanner, false);
	if (!object || errorReporter.hasErrors())
	{
		std::cerr << "Parse errors in " << _path << " (" << errors.size() << " error(s))\n";
		return nullptr;
	}
	return object;
}

class ScopedBimap
{
public:
	void pushScope() { m_scopeStack.emplace_back(); }

	void popScope()
	{
		for (auto const& [l, r] : m_scopeStack.back())
		{
			m_leftToRight.erase(l);
			m_rightToLeft.erase(r);
		}
		m_scopeStack.pop_back();
	}

	/// Try to register a mapping l <-> r.
	/// Returns true if consistent (either new or already matches).
	bool tryMap(YulName _l, YulName _r)
	{
		auto itL = m_leftToRight.find(_l);
		auto itR = m_rightToLeft.find(_r);
		bool lMapped = itL != m_leftToRight.end();
		bool rMapped = itR != m_rightToLeft.end();

		if (lMapped && rMapped)
			return itL->second == _r && itR->second == _l;
		if (lMapped || rMapped)
			return false; // one side mapped but not to each other

		m_leftToRight[_l] = _r;
		m_rightToLeft[_r] = _l;
		m_scopeStack.back().emplace_back(_l, _r);
		return true;
	}

private:
	std::map<YulName, YulName> m_leftToRight;
	std::map<YulName, YulName> m_rightToLeft;
	std::vector<std::vector<std::pair<YulName, YulName>>> m_scopeStack;
};

// ---------------------------------------------------------------------------
// Pretty-print helpers for showing diffs
// ---------------------------------------------------------------------------

static std::string printStatement(Dialect const& _dialect, Statement const& _stmt)
{
	AsmPrinter printer(_dialect, std::nullopt, DebugInfoSelection::None());
	return std::visit([&](auto const& s) { return printer(s); }, _stmt);
}

static std::string printExpression(Dialect const& _dialect, Expression const& _expr)
{
	AsmPrinter printer(_dialect, std::nullopt, DebugInfoSelection::None());
	return std::visit([&](auto const& e) { return printer(e); }, _expr);
}

// ---------------------------------------------------------------------------
// AST Comparator
// ---------------------------------------------------------------------------

class ASTComparator
{
public:
	explicit ASTComparator(Dialect const& _dialect): m_dialect(_dialect) {}

	bool compareObjects(Object const& _a, Object const& _b);

	std::string const& mismatchPath() const { return m_mismatchPath; }
	std::string const& mismatchReason() const { return m_mismatchReason; }
	std::string const& mismatchLHS() const { return m_mismatchLHS; }
	std::string const& mismatchRHS() const { return m_mismatchRHS; }

private:
	void pushPath(std::string _segment) { m_pathStack.push_back(std::move(_segment)); }
	void popPath() { m_pathStack.pop_back(); }
	std::string currentPath() const
	{
		std::string result;
		for (auto const& seg : m_pathStack)
		{
			if (!result.empty()) result += " > ";
			result += seg;
		}
		return result;
	}

	bool fail(std::string _reason)
	{
		m_mismatchPath = currentPath();
		m_mismatchReason = std::move(_reason);
		return false;
	}

	bool fail(std::string _reason, Statement const& _a, Statement const& _b)
	{
		m_mismatchPath = currentPath();
		m_mismatchReason = std::move(_reason);
		m_mismatchLHS = printStatement(m_dialect, _a);
		m_mismatchRHS = printStatement(m_dialect, _b);
		return false;
	}

	bool fail(std::string _reason, Expression const& _a, Expression const& _b)
	{
		m_mismatchPath = currentPath();
		m_mismatchReason = std::move(_reason);
		m_mismatchLHS = printExpression(m_dialect, _a);
		m_mismatchRHS = printExpression(m_dialect, _b);
		return false;
	}

	bool compareBlocks(Block const& _a, Block const& _b);
	bool compareStatements(Statement const& _a, Statement const& _b);
	bool compareExpressions(Expression const& _a, Expression const& _b);
	bool compareFunctionNames(FunctionName const& _a, FunctionName const& _b);
	bool compareIdentifiers(Identifier const& _a, Identifier const& _b);
	bool compareLiterals(Literal const& _a, Literal const& _b);

	Dialect const& m_dialect;
	ScopedBimap m_bimap;
	std::vector<std::string> m_pathStack;
	std::string m_mismatchPath;
	std::string m_mismatchReason;
	std::string m_mismatchLHS;
	std::string m_mismatchRHS;
};

bool ASTComparator::compareObjects(Object const& _a, Object const& _b)
{
	pushPath("Object(\"" + _a.name + "\"/\"" + _b.name + "\")");

	if (_a.subObjects.size() != _b.subObjects.size())
		return fail("different number of sub-objects (" +
			std::to_string(_a.subObjects.size()) + " vs " +
			std::to_string(_b.subObjects.size()) + ")");

	if (_a.hasCode() != _b.hasCode())
		return fail("one has code, the other does not");

	if (_a.hasCode())
	{
		m_bimap.pushScope();
		pushPath("code");
		bool ok = compareBlocks(_a.code()->root(), _b.code()->root());
		popPath();
		m_bimap.popScope();
		if (!ok) return false;
	}

	for (size_t i = 0; i < _a.subObjects.size(); ++i)
	{
		auto const* subA = dynamic_cast<Object const*>(_a.subObjects[i].get());
		auto const* subB = dynamic_cast<Object const*>(_b.subObjects[i].get());

		if ((subA != nullptr) != (subB != nullptr))
			return fail("sub-object[" + std::to_string(i) + "]: type mismatch");

		// todo: this ignores data
		if (subA && subB)
		{
			if (!compareObjects(*subA, *subB))
				return false;
		}
	}

	popPath();
	return true;
}

bool ASTComparator::compareBlocks(Block const& _a, Block const& _b)
{
	m_bimap.pushScope();
	size_t const commonSize = std::min(_a.statements.size(), _b.statements.size());
	for (size_t i = 0; i < commonSize; ++i)
	{
		pushPath("stmt[" + std::to_string(i) + "]");
		if (!compareStatements(_a.statements[i], _b.statements[i]))
		{
			m_bimap.popScope();
			return false;
		}
		popPath();
	}
	if (_a.statements.size() != _b.statements.size())
	{
		pushPath("stmt[" + std::to_string(commonSize) + "]");
		std::string reason = "block statement count differs (" +
			std::to_string(_a.statements.size()) + " vs " +
			std::to_string(_b.statements.size()) + "); first " +
			std::to_string(commonSize) + " statement(s) match";
		m_mismatchPath = currentPath();
		m_mismatchReason = std::move(reason);
		if (commonSize < _a.statements.size())
			m_mismatchLHS = printStatement(m_dialect, _a.statements[commonSize]);
		else
			m_mismatchLHS = "(end of block)";
		if (commonSize < _b.statements.size())
			m_mismatchRHS = printStatement(m_dialect, _b.statements[commonSize]);
		else
			m_mismatchRHS = "(end of block)";
		popPath();
		m_bimap.popScope();
		return false;
	}
	m_bimap.popScope();
	return true;
}

bool ASTComparator::compareStatements(Statement const& _a, Statement const& _b)
{
	if (_a.index() != _b.index())
		return fail("statement type mismatch", _a, _b);

	return std::visit([&](auto const& stmtA) -> bool {
		using T = std::decay_t<decltype(stmtA)>;
		auto const& stmtB = std::get<T>(_b);

		if constexpr (std::is_same_v<T, ExpressionStatement>)
		{
			pushPath("ExpressionStatement");
			bool const ok = compareExpressions(stmtA.expression, stmtB.expression);
			popPath();
			return ok;
		}
		else if constexpr (std::is_same_v<T, Assignment>)
		{
			pushPath("Assignment");
			if (stmtA.variableNames.size() != stmtB.variableNames.size())
				return fail("assignment target count differs", _a, _b);
			for (size_t i = 0; i < stmtA.variableNames.size(); ++i)
				if (!compareIdentifiers(stmtA.variableNames[i], stmtB.variableNames[i]))
				{
					m_mismatchLHS = printStatement(m_dialect, _a);
					m_mismatchRHS = printStatement(m_dialect, _b);
					return false;
				}
			if (static_cast<bool>(stmtA.value) != static_cast<bool>(stmtB.value))
				return fail("assignment value presence differs", _a, _b);
			if (stmtA.value && !compareExpressions(*stmtA.value, *stmtB.value))
				return false;
			popPath();
			return true;
		}
		else if constexpr (std::is_same_v<T, VariableDeclaration>)
		{
			pushPath("VariableDeclaration");
			if (stmtA.variables.size() != stmtB.variables.size())
				return fail("variable declaration count differs", _a, _b);
			for (size_t i = 0; i < stmtA.variables.size(); ++i)
				if (!m_bimap.tryMap(stmtA.variables[i].name, stmtB.variables[i].name))
					return fail("variable name mapping inconsistent: \"" +
						stmtA.variables[i].name.str() + "\" vs \"" +
						stmtB.variables[i].name.str() + "\"", _a, _b);
			if (static_cast<bool>(stmtA.value) != static_cast<bool>(stmtB.value))
				return fail("variable declaration value presence differs", _a, _b);
			if (stmtA.value && !compareExpressions(*stmtA.value, *stmtB.value))
				return false;
			popPath();
			return true;
		}
		else if constexpr (std::is_same_v<T, FunctionDefinition>)
		{
			pushPath("FunctionDefinition(\"" + stmtA.name.str() + "\"/\"" + stmtB.name.str() + "\")");
			if (!m_bimap.tryMap(stmtA.name, stmtB.name))
				return fail("function name mapping inconsistent");
			if (stmtA.parameters.size() != stmtB.parameters.size())
				return fail("parameter count differs (" +
					std::to_string(stmtA.parameters.size()) + " vs " +
					std::to_string(stmtB.parameters.size()) + ")");
			if (stmtA.returnVariables.size() != stmtB.returnVariables.size())
				return fail("return variable count differs (" +
					std::to_string(stmtA.returnVariables.size()) + " vs " +
					std::to_string(stmtB.returnVariables.size()) + ")");
			m_bimap.pushScope();
			for (size_t i = 0; i < stmtA.parameters.size(); ++i)
				if (!m_bimap.tryMap(stmtA.parameters[i].name, stmtB.parameters[i].name))
					return fail("parameter name mapping inconsistent");
			for (size_t i = 0; i < stmtA.returnVariables.size(); ++i)
				if (!m_bimap.tryMap(stmtA.returnVariables[i].name, stmtB.returnVariables[i].name))
					return fail("return variable name mapping inconsistent");
			bool const ok = compareBlocks(stmtA.body, stmtB.body);
			m_bimap.popScope();
			if (!ok) return false;
			popPath();
			return true;
		}
		else if constexpr (std::is_same_v<T, If>)
		{
			pushPath("If");
			if (!compareExpressions(*stmtA.condition, *stmtB.condition))
				return false;
			if (!compareBlocks(stmtA.body, stmtB.body))
				return false;
			popPath();
			return true;
		}
		else if constexpr (std::is_same_v<T, Switch>)
		{
			pushPath("Switch");
			if (!compareExpressions(*stmtA.expression, *stmtB.expression))
				return false;
			if (stmtA.cases.size() != stmtB.cases.size())
				return fail("case count differs (" +
					std::to_string(stmtA.cases.size()) + " vs " +
					std::to_string(stmtB.cases.size()) + ")");
			for (size_t i = 0; i < stmtA.cases.size(); ++i)
			{
				pushPath("case[" + std::to_string(i) + "]");
				if (static_cast<bool>(stmtA.cases[i].value) != static_cast<bool>(stmtB.cases[i].value))
					return fail("case value presence differs (default vs non-default)");
				if (stmtA.cases[i].value)
					if (!compareLiterals(*stmtA.cases[i].value, *stmtB.cases[i].value))
						return false;
				if (!compareBlocks(stmtA.cases[i].body, stmtB.cases[i].body))
					return false;
				popPath();
			}
			popPath();
			return true;
		}
		else if constexpr (std::is_same_v<T, ForLoop>)
		{
			pushPath("ForLoop");
			m_bimap.pushScope();
			if (!compareBlocks(stmtA.pre, stmtB.pre))
			{
				m_bimap.popScope();
				return false;
			}
			if (!compareExpressions(*stmtA.condition, *stmtB.condition))
			{
				m_bimap.popScope();
				return false;
			}
			if (!compareBlocks(stmtA.post, stmtB.post))
			{
				m_bimap.popScope();
				return false;
			}
			if (!compareBlocks(stmtA.body, stmtB.body))
			{
				m_bimap.popScope();
				return false;
			}
			m_bimap.popScope();
			popPath();
			return true;
		}
		else if constexpr (std::is_same_v<T, Break>)
			return true;
		else if constexpr (std::is_same_v<T, Continue>)
			return true;
		else if constexpr (std::is_same_v<T, Leave>)
			return true;
		else if constexpr (std::is_same_v<T, Block>)
		{
			pushPath("Block");
			bool const ok = compareBlocks(stmtA, stmtB);
			popPath();
			return ok;
		}
		else
		{
			return fail("unknown statement type");
		}
	}, _a);
}

bool ASTComparator::compareExpressions(Expression const& _a, Expression const& _b)
{
	if (_a.index() != _b.index())
		return fail("expression type mismatch", _a, _b);

	return std::visit([&](auto const& exprA) -> bool {
		using T = std::decay_t<decltype(exprA)>;
		auto const& exprB = std::get<T>(_b);

		if constexpr (std::is_same_v<T, FunctionCall>)
		{
			if (!compareFunctionNames(exprA.functionName, exprB.functionName))
			{
				if (m_mismatchLHS.empty())
				{
					m_mismatchLHS = printExpression(m_dialect, _a);
					m_mismatchRHS = printExpression(m_dialect, _b);
				}
				return false;
			}
			if (exprA.arguments.size() != exprB.arguments.size())
				return fail("argument count differs", _a, _b);
			for (size_t i = 0; i < exprA.arguments.size(); ++i)
			{
				pushPath("arg[" + std::to_string(i) + "]");
				if (!compareExpressions(exprA.arguments[i], exprB.arguments[i]))
				{
					popPath();
					return false;
				}
				popPath();
			}
			return true;
		}
		else if constexpr (std::is_same_v<T, Identifier>)
		{
			if (!compareIdentifiers(exprA, exprB))
			{
				m_mismatchLHS = printExpression(m_dialect, _a);
				m_mismatchRHS = printExpression(m_dialect, _b);
				return false;
			}
			return true;
		}
		else if constexpr (std::is_same_v<T, Literal>)
		{
			if (!compareLiterals(exprA, exprB))
			{
				m_mismatchLHS = printExpression(m_dialect, _a);
				m_mismatchRHS = printExpression(m_dialect, _b);
				return false;
			}
			return true;
		}
		else
			return fail("unknown expression type");
	}, _a);
}

bool ASTComparator::compareFunctionNames(FunctionName const& _a, FunctionName const& _b)
{
	if (_a.index() != _b.index())
		return fail("function name type mismatch (builtin vs user-defined)");

	if (auto const* builtinA = std::get_if<BuiltinName>(&_a))
	{
		auto const& builtinB = std::get<BuiltinName>(_b);
		if (!(builtinA->handle == builtinB.handle))
			return fail("builtin function mismatch: \"" +
				m_dialect.builtin(builtinA->handle).name + "\" vs \"" +
				m_dialect.builtin(builtinB.handle).name + "\"");
		return true;
	}
	else
	{
		auto const& idA = std::get<Identifier>(_a);
		auto const& idB = std::get<Identifier>(_b);
		return compareIdentifiers(idA, idB);
	}
}

bool ASTComparator::compareIdentifiers(Identifier const& _a, Identifier const& _b)
{
	if (!m_bimap.tryMap(_a.name, _b.name))
		return fail("identifier mapping inconsistent: \"" + _a.name.str() + "\" vs \"" + _b.name.str() + "\"");
	return true;
}

bool ASTComparator::compareLiterals(Literal const& _a, Literal const& _b)
{
	if (_a.kind != _b.kind)
		return fail("literal kind mismatch");
	if (!(_a.value == _b.value))
		return fail("literal value mismatch");
	return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		std::cerr << "Usage: yulCmpAST <file1.yul> <file2.yul>\n";
		return 2;
	}

	auto objA = parseYulFile(argv[1]);
	auto objB = parseYulFile(argv[2]);

	if (!objA || !objB)
	{
		std::cerr << "Aborting due to parse errors.\n";
		return 2;
	}

	Dialect const* dialect = objA->dialect();
	if (!dialect)
	{
		std::cerr << "No dialect available.\n";
		return 2;
	}

	ASTComparator cmp(*dialect);
	if (cmp.compareObjects(*objA, *objB))
	{
		std::cout << "EQUIVALENT\n";
		return 0;
	}
	else
	{
		std::cout << "MISMATCH\n";
		std::cout << "  at:     " << cmp.mismatchPath() << "\n";
		std::cout << "  reason: " << cmp.mismatchReason() << "\n";
		if (!cmp.mismatchLHS().empty())
		{
			std::cout << "\n  --- LHS ---\n" << cmp.mismatchLHS() << "\n";
			std::cout << "\n  --- RHS ---\n" << cmp.mismatchRHS() << "\n";
		}
		return 1;
	}
}
