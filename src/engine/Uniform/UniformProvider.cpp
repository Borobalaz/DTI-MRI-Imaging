#include "UniformProvider.h"

std::string UniformProvider::ComposeUniformName(const std::string& className,
																								const std::string& fieldName)
{
	return className + "." + fieldName;
}
