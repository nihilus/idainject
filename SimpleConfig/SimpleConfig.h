#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <set>

// SimpleConfigMapConstantIterator
typedef std::map<std::string, std::vector<std::string>>::iterator SCMI;
typedef std::map<std::string, std::vector<std::string>>::const_iterator SCCMI;
typedef std::vector<std::string>::const_iterator SCVCI;
typedef std::vector<std::string>::iterator SCVI;

class SimpleConfig
{
public:
	SimpleConfig(const std::string& fileName, const std::vector<std::string>& sections);
	SimpleConfig(const std::string& fileName, const std::string& section);
	~SimpleConfig();
	std::vector<std::string> getSectionCopy(const std::string& section);
	const std::vector<std::string>& getSection(const std::string& section);
	void writeSection(const std::string& section, const std::vector<std::string>& values);
	void addValue(const std::string& section, const std::string& value);
	void delValue(const std::string& section, const std::string& value);
	void delValue(const std::string& value);
	void flush();

private:
	void parseFile();
	void rewriteFile();
	std::string fileName_;
	std::set<std::string> sections_;
	std::map<std::string, std::vector<std::string>> configMap_;
};