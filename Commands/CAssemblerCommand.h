#pragma once

class TempData;
class SymbolData;

class CAssemblerCommand
{
public:
	CAssemblerCommand();
	virtual ~CAssemblerCommand() { };
	virtual bool Validate() = 0;
	virtual void Encode() const = 0;
	virtual void writeTempData(TempData& tempData) const = 0;
	virtual void writeSymData(SymbolData& symData) const { };
	virtual bool IsConditional() { return false; };
	virtual bool IsPool() { return false; };
	void applyFileInfo();
	int getSection() { return section; }
	void updateSection(int num) { section = num; }
protected:
	int FileNum;
	int FileLine;
private:
	int section;
};

class DummyCommand: public CAssemblerCommand
{
public:
	virtual bool Validate() { return false; };
	virtual void Encode() const { };
	virtual void writeTempData(TempData& tempData) const { };
	virtual void writeSymData(SymbolData& symData) const { };
};

class InvalidCommand: public CAssemblerCommand
{
public:
	virtual bool Validate() { return false; };
	virtual void Encode() const { };
	virtual void writeTempData(TempData& tempData) const { };
	virtual void writeSymData(SymbolData& symData) const { };
};

class CommentCommand: public CAssemblerCommand
{
public:
	CommentCommand(const std::wstring& tempText, const std::wstring& symText);
	virtual bool Validate();
	virtual void Encode() const { };
	virtual void writeTempData(TempData& tempData) const;
	virtual void writeSymData(SymbolData& symData) const;
private:
	u64 position;
	std::wstring tempText;
	std::wstring symText;
};