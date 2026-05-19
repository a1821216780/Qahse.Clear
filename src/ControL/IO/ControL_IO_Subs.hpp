#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "ControL/ControL_Type.hpp"
#include "IO/ModuleIO.hpp"
#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"

namespace control_io_detail
{
inline constexpr const char *kYamlRoot = "Qahse.ControL";

class ControLInputSerializer : public Serializer
{
public:
	ControLInput data;

	ControLInputSerializer()
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

	explicit ControLInputSerializer(const ControLInput &input)
		: data(input)
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

protected:
	void SerializeFields() override
	{
		Fields(Field("PCMode", data.pcMode));
		ReadOrWriteAny({"DLL_FileName", "DLLFileName", "DLLFile"}, data.dllFileName);
		ReadOrWriteAny({"DLL_InFile", "DLLInFile"}, data.dllInFile);
		ReadOrWriteAny({"DLL_ProcName", "DLLProcName"}, data.dllProcName);
		Fields(
			Field("SumPrint", data.output.sumPrint),
			Field("AfSpanput", data.output.afSpanput),
			Field("SumPath", data.output.sumPath));
	}
};

inline void ResolvePaths(ControLInput &input)
{
	const std::string base = input.inputPath.string();
	module_io::ResolveIfSet(base, input.dllFileName);
	module_io::ResolveIfSet(base, input.dllInFile);
	module_io::ResolveOutputPath(base, input.output);
}

inline void Validate(const ControLInput &input)
{
	if (input.pcMode != 0 && !module_io::FileExistsOrEmpty(input.dllFileName))
		throw std::runtime_error("ControL PCMode requires DLL_FileName: " + input.dllFileName);
	if (input.pcMode != 0 && !module_io::FileExistsOrEmpty(input.dllInFile))
		throw std::runtime_error("ControL PCMode requires DLL_InFile: " + input.dllInFile);
}

inline ControLInput ReadControLInput(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open ControL input file: " + path);

	ControLInputSerializer reader;
	reader.ReadFile(path);
	ControLInput input = reader.data;
	input.inputPath = std::filesystem::absolute(path).lexically_normal();
	module_io::ReadOutputConfig(reader, path, kYamlRoot, input.output);
	ResolvePaths(input);
	Validate(input);
	return input;
}

inline void WriteControLInputYaml(const ControLInput &input, const std::string &path)
{
	ControLInputSerializer writer(input);
	writer.WriteYamlFile(path);
	module_io::AddOutputYamlNodes(writer, input.output);
	writer.SaveYamlFile(path);
}

inline void WriteControLInput(const ControLInput &input,
                              const std::string &path,
                              const std::string &templatePath = "")
{
	if (Serializer::IsYamlPath(path))
	{
		WriteControLInputYaml(input, path);
		return;
	}
	ControLInputSerializer writer(input);
	if (!templatePath.empty() && ZFile::Exists(templatePath))
		writer.LoadTextFile(templatePath);
	writer.WriteTextFile(path);
}

inline void ConvertControLInput(const std::string &inputPath,
                                const std::string &outputPath,
                                const std::string &templatePath = "")
{
	WriteControLInput(ReadControLInput(inputPath), outputPath, templatePath);
}
} // namespace control_io_detail

using control_io_detail::ConvertControLInput;
using control_io_detail::ReadControLInput;
using control_io_detail::WriteControLInput;
