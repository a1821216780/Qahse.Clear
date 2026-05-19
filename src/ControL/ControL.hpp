#pragma once

#include <string>

#include "ControL/ControL_Type.hpp"

ControLInput ReadControLInputFile(const std::string &path);

class ControL
{
public:
	static ControLInput ReadInputFile(const std::string &path);
	static void ValidateInputOnly(const ControLInput &input);
	static ControL Load(const ControLInput &input);
	static ControL LoadFromFile(const std::string &path);

	const ControLInput &Input() const;
	std::string Summary() const;

private:
	ControLInput input_;
};
