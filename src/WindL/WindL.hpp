#pragma once

#include <array>
#include <functional>
#include <string>

#include "WindL/WindField.hpp"
#include "WindL/WindL_Type.hpp"

using WindLProgressCallback = std::function<void(const std::string &)>;

WindLInput ReadWindLInput(const std::string &path);

class WindL
{
public:
	static WindLInput ReadInputFile(const std::string &path);
	static void ValidateInputOnly(const WindLInput &input);
	static void ValidateImportInputOnly(const WindImportMetadata &metadata);

	static WindField Import(const WindImportMetadata &metadata, WindLProgressCallback progress = {});
	static WindField ImportFromFile(const std::string &path, WindLProgressCallback progress = {});

	static WindL Load(const WindLInput &input, WindLProgressCallback progress = {});
	static WindL LoadFromFile(const std::string &path, WindLProgressCallback progress = {});

	std::array<double, 3> VelocityAt(double x,
	                                 double y,
	                                 double z,
	                                 double time,
	                                 const WindVelocityOptions &options = {}) const;
	Vec3 getWindspeed(Vec3 vec,
	                  double time,
	                  bool mirror = false,
	                  bool isAutoFielShift = true,
	                  double shiftTime = 0.0) const;

	const WindLInput &Input() const;
	const WindField *ImportedField() const;
	bool HasImportedField() const;

private:
	WindLInput input_;
	WindField importedField_;
	bool hasImportedField_ = false;
};
