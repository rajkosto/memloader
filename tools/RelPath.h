#pragma once

#include <boost/filesystem.hpp>

std::string GetRelativePath(const char* inputFilename, const char* outputFilename)
{
	namespace fs=boost::filesystem;
	auto inputPath = fs::canonical(inputFilename);
	auto outputPath = fs::absolute(outputFilename).remove_filename();
	auto relPath = boost::filesystem::relative(inputPath, outputPath);
	std::string relativePath = relPath.string();
	std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

	return relativePath;
}
