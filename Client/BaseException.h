#pragma once
#include <exception>
#include <string>

class BaseException : public std::exception
{
public:
	BaseException(int line, const char* file);
	const char* what() const override;
	virtual const char* GetType() const;
	int GetLine() const;
	const std::string& GetFile() const;
	std::string GetOriginString() const;
	virtual ~BaseException() = default;
private:
	int line;
	std::string file;
protected:
	mutable std::string whatBuffer;
};

