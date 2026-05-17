#pragma once

#include <functional>
#include <string>

#include "WindL/WindField.hpp"
#include "SiMwind/SimWind_Type.hpp"

using WindLProgressCallback = std::function<void(const std::string &)>;

class WindL
{
public:
	static void ValidateImportInputOnly(const SimWindInput &input);
	static WindField Import(const SimWindInput &input, WindLProgressCallback progress = {});
	static WindField ImportFromFile(const std::string &qwdPath, WindLProgressCallback progress = {});
};
