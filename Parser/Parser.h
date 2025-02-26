#pragma once
#include "Tokenizer.h"
#include "Core/Expression.h"
#include "Commands/CommandSequence.h"
#include "DirectivesParser.h"
#include <set>
#include <map>
#include <unordered_map>
#include "Core/Common.h"

struct AssemblyTemplateArgument
{
	const wchar_t* variableName;
	std::wstring value;
};

struct ParserMacro
{
	std::wstring name;
	std::vector<std::wstring> parameters;
	std::set<std::wstring> labels;
	std::vector<Token> content;
	size_t counter;
};

class Parser
{
public:
	Parser();
	bool atEnd() { return entries.back().tokenizer->atEnd(); }

	void addEquation(const std::wstring& name, const std::wstring& value);

	Expression parseExpression();
	bool parseExpressionList(std::vector<Expression>& list, int min = -1, int max = -1);
	bool parseIdentifier(std::wstring& dest);
	CAssemblerCommand* parseCommand();
	CAssemblerCommand* parseCommandSequence(wchar_t indicator = 0, std::initializer_list<wchar_t*> terminators = {});
	CAssemblerCommand* parseFile(TextFile& file, bool virtualFile = false);
	CAssemblerCommand* parseString(const std::wstring& text);
	CAssemblerCommand* parseTemplate(const std::wstring& text, std::initializer_list<AssemblyTemplateArgument> variables = {});
	CAssemblerCommand* parseDirective(const DirectiveMap &directiveSet);
	bool matchToken(TokenType type, bool optional = false);

	Tokenizer* getTokenizer() { return entries.back().tokenizer; };
	const Token& peekToken(int ahead = 0) { return getTokenizer()->peekToken(ahead); };
	const Token& nextToken() { return getTokenizer()->nextToken(); };
	void eatToken() { getTokenizer()->eatToken(); };
	void eatTokens(int num) { getTokenizer()->eatTokens(num); };
	
	template <typename... Args>
	void printError(const Token& token, const wchar_t* text, const Args&... args)
	{
		Global.FileInfo.LineNumber = (int) token.line;
		std::wstring errorText = formatString(text,args...);
		Logger::printError(Logger::Error,errorText);
		error = true;
	}

	bool hasError() { return error; }
	void updateFileInfo();
protected:
	void clearError() { error = false; }
	CAssemblerCommand* handleError();

	CAssemblerCommand* parse(Tokenizer* tokenizer, bool virtualFile, const std::wstring& name = L"");
	CAssemblerCommand* parseLabel();
	bool parseMacro();
	bool checkEquLabel();
	bool checkMacroDefinition();
	CAssemblerCommand* parseMacroCall();

	struct FileEntry
	{
		Tokenizer* tokenizer;
		bool virtualFile;
		int fileNum;
	};

	std::vector<FileEntry> entries;
	std::map<std::wstring,ParserMacro> macros;
	std::set<std::wstring> macroLabels;
	bool initializingMacro;
	bool error;

	bool overrideFileInfo;
	int overrideFileNum;
	int overrideLineNum;
};

struct TokenSequenceValue
{
	TokenSequenceValue(const wchar_t* text)
	{
		type = TokenType::Identifier;
		textValue = text;
	}
	
	TokenSequenceValue(u64 num)
	{
		type = TokenType::Integer;
		intValue = num;
	}
	
	TokenSequenceValue(double num)
	{
		type = TokenType::Float;
		floatValue = num;
	}
	

	TokenType type;
	union
	{
		const wchar_t* textValue;
		u64 intValue;
		double floatValue;
	};
};

typedef std::initializer_list<TokenType> TokenSequence;
typedef std::initializer_list<TokenSequenceValue> TokenValueSequence;

class TokenSequenceParser
{
public:
	void addEntry(int result, TokenSequence tokens, TokenValueSequence values);
	bool parse(Parser& parser, int& result);
	size_t getEntryCount() { return entries.size(); }
private:
	struct Entry
	{
		std::vector<TokenType> tokens;
		std::vector<TokenSequenceValue> values;
		int result;
	};

	std::vector<Entry> entries;
};
