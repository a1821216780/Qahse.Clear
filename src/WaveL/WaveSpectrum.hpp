#pragma once

#include <string>
#include <vector>

#include "WaveL/WaveL_Type.hpp"

namespace wavel_spectrum
{
	std::vector<WaveComponent> BuildComponents(const WaveLInput &input);
	std::vector<WaveComponent> ReadComponentFile(const std::string &path, const WaveLInput &input);
	void WriteComponentFile(const std::string &path, const std::vector<WaveComponent> &components);
}
