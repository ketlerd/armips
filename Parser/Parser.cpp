#include "stdafx.h"
#include "Parser.h"
#include "ExpressionParser.h"
#include "Core/Misc.h"
#include "Commands/CommandSequence.h"
#include "Commands/CAssemblerLabel.h"
#include "Core/Common.h"
#include "Util/Util.h"

inline bool isPartOfList(const std::wstring& value, std::initializer_list<wchar_t*>& terminators)
{
	for (wchar_t* term: terminators)
	{
		if (value == term)
			return true;
	}

	return false;
}

Parser::Parser()
{
	initializingMacro = false;
	overrideFileInfo = false;
	clearError();
}

Expression Parser::parseExpression()
{
	return ::parseExpression(*getTokenizer());
}

bool Parser::parseExpressionList(std::vector<Expression>& list, int min, int max)
{
	bool valid = true;
	list.clear();

	const Token& start = peekToken();

	Expression exp = parseExpression();
	list.push_back(exp);

	if (exp.isLoaded() == false)
	{
		printError(start,L"Parameter failure");
		getTokenizer()->skipLookahead();
		valid = false;
	}

	while (peekToken().type == TokenType::Comma)
	{
		eatToken();

		exp = parseExpression();
		list.push_back(exp);

		if (exp.isLoaded() == false)
		{
			printError(start,L"Parameter failure");
			getTokenizer()->skipLookahead();
			valid = false;
		}
	}

	if (list.size() < (size_t) min)
	{
		printError(start,L"Not enough parameters (min %d)",min);
		return false;
	}

	if (max != -1 && (size_t) max < list.size())
	{
		printError(start,L"Too many parameters (max %d)",max);
		return false;
	}

	return valid;
}

bool Parser::parseIdentifier(std::wstring& dest)
{
	const Token& tok = nextToken();
	if (tok.type != TokenType::Identifier)
		return false;

	dest = tok.getStringValue();
	return true;
}

CAssemblerCommand* Parser::parseCommandSequence(wchar_t indicator, std::initializer_list<wchar_t*> terminators)
{
	CommandSequence* sequence = new CommandSequence();

	while (atEnd() == false)
	{
		const Token &next = peekToken();
		if (next.stringValueStartsWith(indicator) && isPartOfList(next.getStringValue(), terminators))
			break;

		CAssemblerCommand* cmd = parseCommand();
		sequence->addCommand(cmd);
	}

	return sequence;
}

CAssemblerCommand* Parser::parseFile(TextFile& file, bool virtualFile)
{
	FileTokenizer tokenizer;
	if (tokenizer.init(&file) == false)
		return nullptr;

	CAssemblerCommand* result = parse(&tokenizer,virtualFile,file.getFileName());

	if (file.isFromMemory() == false)
		Global.FileInfo.TotalLineCount += file.getNumLines();

	return result;
}

CAssemblerCommand* Parser::parseString(const std::wstring& text)
{
	TextFile file;
	file.openMemory(text);
	return parseFile(file,true);
}

CAssemblerCommand* Parser::parseTemplate(const std::wstring& text, std::initializer_list<AssemblyTemplateArgument> variables)
{
	std::wstring fullText = text;

	overrideFileInfo = true;
	overrideFileNum = Global.FileInfo.FileNum;
	overrideLineNum = Global.FileInfo.LineNumber;

	for (auto& arg: variables)
	{
		size_t count = replaceAll(fullText,arg.variableName,arg.value);

#ifdef _DEBUG
		if (count != 0 && arg.value.empty())
			Logger::printError(Logger::Warning,L"Empty replacement for %s",arg.variableName);
#endif
	}

	CAssemblerCommand* result = parseString(fullText);
	overrideFileInfo = false;

	return result;
}

CAssemblerCommand* Parser::parseDirective(const DirectiveMap &directiveSet)
{
	const Token &tok = peekToken();
	if (tok.type != TokenType::Identifier)
		return nullptr;

	const std::wstring stringValue = tok.getStringValue();

	auto matchRange = directiveSet.equal_range(stringValue);
	for (auto it = matchRange.first; it != matchRange.second; ++it)
	{
		const DirectiveEntry &directive = it->second;

		if (directive.flags & DIRECTIVE_DISABLED)
			continue;
		if ((directive.flags & DIRECTIVE_NOCASHOFF) && Global.nocash == true)
			continue;
		if ((directive.flags & DIRECTIVE_NOCASHON) && Global.nocash == false)
			continue;
		if ((directive.flags & DIRECTIVE_NOTINMEMORY) && Global.memoryMode == true)
			continue;

		if (directive.flags & DIRECTIVE_MIPSRESETDELAY)
			Arch->NextSection();

		eatToken();
		CAssemblerCommand* result = directive.function(*this,directive.flags);
		if (result == nullptr)
		{
			if (hasError() == false)
				printError(tok,L"Directive parameter failure");
			return nullptr;
		}
		
		return result;
	}

	return nullptr;
}

bool Parser::matchToken(TokenType type, bool optional)
{
	if (optional)
	{
		const Token& token = peekToken();
		if (token.type == type)
			eatToken();
		return true;
	}
	
	return nextToken().type == type;
}

CAssemblerCommand* Parser::parse(Tokenizer* tokenizer, bool virtualFile, const std::wstring& name)
{
	FileEntry entry;
	entry.tokenizer = tokenizer;
	entry.virtualFile = virtualFile;

	if (virtualFile == false && name.empty() == false)
	{
		entry.fileNum = (int) Global.FileInfo.FileList.size();
		Global.FileInfo.FileList.push_back(name);
	} else {
		entry.fileNum = -1;
	}

	entries.push_back(entry);

	CAssemblerCommand* sequence = parseCommandSequence();
	entries.pop_back();

	return sequence;
}

void Parser::addEquation(const std::wstring& name, const std::wstring& value)
{
	// parse value string
	TextFile f;
	f.openMemory(value);

	FileTokenizer tok;
	tok.init(&f);

	TokenizerPosition start = tok.getPosition();
	while (tok.atEnd() == false)
		tok.nextToken();

	// extract tokens
	TokenizerPosition end = tok.getPosition();
	std::vector<Token> tokens = tok.getTokens(start, end);
	size_t index = Tokenizer::addEquValue(tokens);

	for (FileEntry& entry : entries)
		entry.tokenizer->resetLookaheadCheckMarks();

	// register equation
	Global.symbolTable.addEquation(name, Global.FileInfo.FileNum, Global.Section, index);
}

bool Parser::checkEquLabel()
{
	updateFileInfo();

	const Token& start = peekToken();
	if (start.type == TokenType::Identifier)
	{
		int pos = 1;
		if (peekToken(pos).type == TokenType::Colon)
			pos++;

		if (peekToken(pos).type == TokenType::Equ &&
			peekToken(pos+1).type == TokenType::EquValue)
		{
			std::wstring name = peekToken(0).getStringValue();
			std::wstring value = peekToken(pos+1).getStringValue();
			eatTokens(pos+2);
		
			// equs are not allowed in macros
			if (initializingMacro)
			{
				printError(start,L"equ not allowed in macro");
				return true;
			}

			if (Global.symbolTable.isValidSymbolName(name) == false)
			{
				printError(start,L"Invalid equation name %s",name);
				return true;
			}

			if (Global.symbolTable.symbolExists(name,Global.FileInfo.FileNum,Global.Section))
			{
				printError(start,L"Equation name %s already defined",name);
				return true;
			}

			addEquation(name,value);
			return true;
		}
	}

	return false;
}

bool Parser::checkMacroDefinition()
{
	const Token& first = peekToken();
	if (first.type != TokenType::Identifier)
		return false;

	if (!first.stringValueStartsWith(L'.') || first.getStringValue() != L".macro")
		return false;

	eatToken();

	// nested macro definitions are not allowed
	if (initializingMacro)
	{
		printError(first,L"Nested macro definitions not allowed");
		while (!atEnd())
		{
			const Token& token = nextToken();
			if (token.type == TokenType::Identifier && token.getStringValue() == L".endmacro")
				break;
		}

		return true;
	}

	std::vector<Expression> parameters;
	if (parseExpressionList(parameters,1,-1) == false)
		return false;
	
	// load name
	std::wstring macroName;
	if (parameters[0].evaluateIdentifier(macroName) == false)
		return false;

	// duplicate check the macro
	ParserMacro &macro = macros[macroName];
	if (macro.name.length() != 0)
	{
		printError(first,L"Macro \"%s\" already defined",macro.name);
		return false;
	}

	// and register it
	macro.name = macroName;
	macro.counter = 0;

	// load parameters
	for (size_t i = 1; i < parameters.size(); i++)
	{
		std::wstring name;
		if (parameters[i].evaluateIdentifier(name) == false)
			return false;

		macro.parameters.push_back(name);
	}

	// load macro content

	TokenizerPosition start = getTokenizer()->getPosition();
	bool valid = false;
	while (atEnd() == false)
	{
		const Token& tok = nextToken();
		if (tok.type == TokenType::Identifier && tok.getStringValue() == L".endmacro")
		{
			valid = true;
			break;
		}
	}
	
	// no .endmacro, not valid
	if (valid == false)
		return true;

	// get content
	TokenizerPosition end = getTokenizer()->getPosition().previous();
	macro.content = getTokenizer()->getTokens(start,end);

	return true;
}

CAssemblerCommand* Parser::parseMacroCall()
{
	const Token& start = peekToken();
	if (start.type != TokenType::Identifier)
		return nullptr;

	auto it = macros.find(start.getStringValue());
	if (it == macros.end())
		return nullptr;

	ParserMacro& macro = it->second;
	eatToken();

	// create a token stream for the macro content,
	// registering replacements for parameter values
	TokenStreamTokenizer macroTokenizer;

	std::set<std::wstring> identifierParameters;
	for (size_t i = 0; i < macro.parameters.size(); i++)
	{
		if (i != 0)
		{
			if (nextToken().type != TokenType::Comma)
				return nullptr;
		}

		if (i == macro.parameters.size())
		{
			size_t count = macro.parameters.size();
			while (peekToken().type == TokenType::Comma)
			{
				eatToken();
				parseExpression();
			}

			printError(start,L"Not enough macro arguments (%d vs %d)",count,macro.parameters.size());		
			return nullptr;
		}

		TokenizerPosition startPos = getTokenizer()->getPosition();
		Expression exp = parseExpression();
		if (exp.isLoaded() == false)
			return nullptr;

		TokenizerPosition endPos = getTokenizer()->getPosition();
		std::vector<Token> tokens = getTokenizer()->getTokens(startPos,endPos);

		// remember any single identifier parameters for the label replacement
		if (tokens.size() == 1 && tokens[0].type == TokenType::Identifier)
			identifierParameters.insert(tokens[0].getStringValue());

		// give them as a replacement to new tokenizer
		macroTokenizer.registerReplacement(macro.parameters[i],tokens);
	}

	if (peekToken().type == TokenType::Comma)
	{
		size_t count = macro.parameters.size();
		while (peekToken().type == TokenType::Comma)
		{
			eatToken();
			parseExpression();
			count++;
		}

		printError(start,L"Too many macro arguments (%d vs %d)",count,macro.parameters.size());		
		return nullptr;
	}

	// a macro is fully parsed once when it's loaded
	// to gather all labels. it's not necessary to
	// instantiate other macros at that time
	if (initializingMacro)
		return new DummyCommand();

	// the first time a macro is instantiated, it needs to be analyzed
	// for labels
	if (macro.counter == 0)
	{
		initializingMacro = true;
		
		// parse the short lived next command
		macroTokenizer.init(macro.content);
		CAssemblerCommand* command =  parse(&macroTokenizer,true);
		delete command;

		macro.labels = macroLabels;
		macroLabels.clear();
		
		initializingMacro = false;
	}

	// register labels and replacements
	for (const std::wstring& label: macro.labels)
	{
		// check if the label is using the name of a parameter
		// in that case, don't register a unique replacement
		if (identifierParameters.find(label) != identifierParameters.end())
			continue;

		// otherwise make sure the name is unique
		std::wstring fullName;
		if (Global.symbolTable.isLocalSymbol(label))
			fullName = formatString(L"@@%s_%s_%08X",macro.name,label.substr(2),macro.counter);
		else if (Global.symbolTable.isStaticSymbol(label))
			fullName = formatString(L"@%s_%s_%08X",macro.name,label.substr(1),macro.counter);
		else
			fullName = formatString(L"%s_%s_%08X",macro.name,label,macro.counter);

		macroTokenizer.registerReplacement(label,fullName);
	}
	
	macroTokenizer.init(macro.content);
	macro.counter++;

	return parse(&macroTokenizer,true);
}

CAssemblerCommand* Parser::parseLabel()
{
	updateFileInfo();

	const Token& start = peekToken(0);

	if (peekToken(0).type == TokenType::Identifier &&
		peekToken(1).type == TokenType::Colon)
	{
		const std::wstring name = peekToken(0).getStringValue();
		eatTokens(2);
		
		if (initializingMacro)
			macroLabels.insert(name);
		
		if (Global.symbolTable.isValidSymbolName(name) == false)
		{
			printError(start,L"Invalid label name");
			return nullptr;
		}

		return new CAssemblerLabel(name);
	}

	return nullptr;
}

CAssemblerCommand* Parser::handleError()
{
	// skip the rest of the line
	const Token& token = nextToken();
	while (peekToken().line == token.line)
		eatToken();

	clearError();
	return new InvalidCommand();
}


void Parser::updateFileInfo()
{
	if (overrideFileInfo)
	{
		Global.FileInfo.FileNum = overrideFileNum;
		Global.FileInfo.LineNumber = overrideLineNum;
		return;
	}

	for (size_t i = entries.size(); i > 0; i--)
	{
		size_t index = i-1;

		if (entries[index].virtualFile == false && entries[index].fileNum != -1)
		{
			Global.FileInfo.FileNum = entries[index].fileNum;
			Global.FileInfo.LineNumber = (int)entries[index].tokenizer->peekToken().line;
			return;
		}
	}
}

CAssemblerCommand* Parser::parseCommand()
{
	CAssemblerCommand* command;

	while (checkEquLabel() || checkMacroDefinition())
	{
		// do nothing, just parse all the equs and macros there are
		if (hasError())
			return handleError();
	}
	
	updateFileInfo();

	if (atEnd())
		return new DummyCommand();
	
	if ((command = parseLabel()) != nullptr)
		return command;
	if (hasError())
		return handleError();

	if ((command = parseMacroCall()) != nullptr)
		return command;
	if (hasError())
		return handleError();

	if ((command = Arch->parseDirective(*this)) != nullptr)
		return command;
	if (hasError())
		return handleError();

	if ((command = parseDirective(directives)) != nullptr)
		return command;
	if (hasError())
		return handleError();

	if ((command = Arch->parseOpcode(*this)) != nullptr)
		return command;
	if (hasError())
		return handleError();

	const Token& token = peekToken();
	printError(token,L"Parse error '%s'",token.getOriginalText());
	return handleError();
}

void TokenSequenceParser::addEntry(int result, TokenSequence tokens, TokenValueSequence values)
{
	Entry entry = { tokens, values, result };
	entries.push_back(entry);
}

bool TokenSequenceParser::parse(Parser& parser, int& result)
{
	for (Entry& entry: entries)
	{
		TokenizerPosition pos = parser.getTokenizer()->getPosition();
		auto values = entry.values.begin();

		bool valid = true;
		for (TokenType type: entry.tokens)
		{
			// check of token type matches
			const Token& token = parser.nextToken();
			if (token.type != type)
			{
				valid = false;
				break;
			}

			// if necessary, check if the value of the token also matches
			if (type == TokenType::Identifier)
			{
				if (values == entry.values.end() || values->textValue != token.getStringValue())
				{
					valid = false;
					break;
				}
				
				values++;
			} else if (type == TokenType::Integer)
			{
				if (values == entry.values.end() || values->intValue != token.intValue)
				{
					valid = false;
					break;
				}
				
				values++;
			} 
		}

		if (valid && values == entry.values.end())
		{
			result = entry.result;
			return true;
		}

		parser.getTokenizer()->setPosition(pos);
	}

	return false;
}
