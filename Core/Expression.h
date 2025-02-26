#pragma once
#include <memory>

inline std::wstring to_wstring(u64 value)
{
	return formatString(L"%d", value);
}

inline std::wstring to_wstring(double value)
{
	return formatString(L"%f", value);
}

enum class OperatorType
{
	Invalid,
	Integer,
	Float,
	Identifier,
	String,
	MemoryPos,
	Add,
	Sub,
	Mult,
	Div,
	Mod,
	Neg,
	LogNot,
	BitNot,
	LeftShift,
	RightShift,
	Less,
	Greater,
	LessEqual,
	GreaterEqual,
	Equal,
	NotEqual,
	BitAnd,
	Xor,
	BitOr,
	LogAnd,
	LogOr,
	TertiaryIf,
	ToString
};

enum class ExpressionValueType { Invalid, Integer, Float, String};

struct ExpressionValue
{
	ExpressionValueType type;

	ExpressionValue()
	{
		type = ExpressionValueType::Invalid;
	}

	bool isFloat() const
	{
		return type == ExpressionValueType::Float;
	}
	
	bool isInt() const
	{
		return type == ExpressionValueType::Integer;
	}

	bool isString() const
	{
		return type == ExpressionValueType::String;
	}

	bool isValid() const
	{
		return type != ExpressionValueType::Invalid;
	}

	struct
	{
		u64 intValue;
		double floatValue;
	};

	std::wstring strValue;
	
	ExpressionValue operator!() const;
	ExpressionValue operator~() const;
	bool operator<(const ExpressionValue& other) const;
	bool operator<=(const ExpressionValue& other) const;
	bool operator>(const ExpressionValue& other) const;
	bool operator>=(const ExpressionValue& other) const;
	bool operator==(const ExpressionValue& other) const;
	bool operator!=(const ExpressionValue& other) const;
	ExpressionValue operator+(const ExpressionValue& other) const;
	ExpressionValue operator-(const ExpressionValue& other) const;
	ExpressionValue operator*(const ExpressionValue& other) const;
	ExpressionValue operator/(const ExpressionValue& other) const;
	ExpressionValue operator%(const ExpressionValue& other) const;
	ExpressionValue operator<<(const ExpressionValue& other) const;
	ExpressionValue operator>>(const ExpressionValue& other) const;
	ExpressionValue operator&(const ExpressionValue& other) const;
	ExpressionValue operator|(const ExpressionValue& other) const;
	ExpressionValue operator&&(const ExpressionValue& other) const;
	ExpressionValue operator||(const ExpressionValue& other) const;
	ExpressionValue operator^(const ExpressionValue& other) const;
};

class Label;

class ExpressionInternal
{
public:
	ExpressionInternal();
	ExpressionInternal(u64 value);
	ExpressionInternal(double value);
	ExpressionInternal(const std::wstring& value, OperatorType type);
	ExpressionInternal(OperatorType op, ExpressionInternal* a = NULL,
		ExpressionInternal* b = NULL, ExpressionInternal* c = NULL);
	ExpressionValue evaluate();
	std::wstring toString();
	bool hasIdentifierChild();
	bool isIdentifier() { return type == OperatorType::Identifier; }
	std::wstring getStringValue() { return strValue; }
	void replaceMemoryPos(const std::wstring& identifierName);
private:
	OperatorType type;
	ExpressionInternal* children[3];
	union
	{
		u64 intValue;
		double floatValue;
	};
	std::wstring strValue;

	unsigned int fileNum, section;
};

class Expression
{
public:
	Expression();
	ExpressionValue evaluate();
	bool isLoaded() const { return expression != NULL; }
	void setExpression(ExpressionInternal* exp) { expression = std::shared_ptr<ExpressionInternal>(exp); }
	void replaceMemoryPos(const std::wstring& identifierName);

	template<typename T>
	bool evaluateInteger(T& dest)
	{
		if (expression == NULL)
			return false;

		ExpressionValue value = expression->evaluate();
		if (value.isInt() == false)
			return false;

		dest = (T) value.intValue;
		return true;
	}

	bool evaluateString(std::wstring& dest, bool convert)
	{
		if (expression == NULL)
			return false;

		ExpressionValue value = expression->evaluate();
		if (convert && value.isInt())
		{
			dest = to_wstring(value.intValue);
			return true;
		}

		if (convert && value.isFloat())
		{
			dest = to_wstring(value.floatValue);
			return true;
		}

		if (value.isString() == false)
			return false;

		dest = value.strValue;
		return true;
	}
	
	bool evaluateIdentifier(std::wstring& dest)
	{
		if (expression == NULL || expression->isIdentifier() == false)
			return false;

		dest = expression->getStringValue();
		return true;
	}

	std::wstring toString() { return expression != NULL ? expression->toString() : L""; };
private:
	std::shared_ptr<ExpressionInternal> expression;
	std::wstring originalText;
};

Expression createConstExpression(u64 value);