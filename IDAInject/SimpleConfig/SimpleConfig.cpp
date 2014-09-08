#include "SimpleConfig.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

using namespace std;

typedef vector<string>::const_iterator VCIT;
typedef set<string>::const_iterator SCIT;

SimpleConfig::SimpleConfig(const string& fileName, const std::vector<std::string>& sections)
	: fileName_(fileName), sections_(sections.begin(), sections.end())
{
	parseFile();
}

SimpleConfig::SimpleConfig(const std::string& fileName, const std::string& section)
	: fileName_(fileName)
{
	sections_.insert(section);
	parseFile();
}

SimpleConfig::~SimpleConfig()
{
}

void SimpleConfig::parseFile()
{
	ifstream ifs(fileName_.c_str());
	string line;
	string currentSection;
	
	vector<string> lines;
	while(getline(ifs, line))
	{
		if (!line.empty())
			if (line[0] != ';') lines.push_back(line);
	}

	// init map with empty value vectors
	for (SCIT sCit = sections_.begin(); sCit != sections_.end(); ++sCit) 
		configMap_.insert(make_pair(*sCit, vector<string>()));

	SCVCI citLines = lines.begin();
	bool ignore = false;
	SCMI currVecIt = configMap_.end();
	for (; citLines != lines.end(); ++citLines)
	{
		// first check if this section is valid anyway
		SCIT sCit = sections_.find(*citLines);
		if (sCit != sections_.end())
		{
			// check if section is already in configMap, if not add new entry
			currentSection = *citLines;
			ignore = false;
			pair<SCMI, bool> p = configMap_.insert(make_pair(*citLines, vector<string>()));
			currVecIt = p.first;
		}
		else
		{
			// if this is a non-recognized section, enter ignore mode until we spot a known section again
			if ((*citLines)[0] == '[') ignore = true;
			else if (!ignore && !configMap_.empty()) currVecIt->second.push_back(*citLines);
		}
	}
}

// get only one section
vector<string> SimpleConfig::getSectionCopy(const string& section)
{
	SCCMI result = configMap_.find(section);
	vector<string> dlls(result->second.begin(), result->second.end());
	return dlls;
}

const vector<string>& SimpleConfig::getSection(const std::string& section)
{
	return configMap_.find(section)->second;
}

void SimpleConfig::writeSection(const std::string& section, const std::vector<std::string>& values)
{
	// erase old values in this section first, then populate with new ones
	SCMI vCit = configMap_.find(section);
	vCit->second.clear();
	vCit->second.assign(values.begin(), values.end());
	rewriteFile();
}

// iterate over all sections / values and write them to file
void SimpleConfig::rewriteFile()
{
	ofstream ofs(fileName_.c_str(), ios_base::out);
	for (SCIT sCit=sections_.begin(); sCit!=sections_.end(); ++sCit)
	{
		if (sCit != sections_.begin()) ofs << endl << endl;
		ofs << *sCit;
		const vector<string>& vec = configMap_.find(*sCit)->second;
		for (VCIT vCit=vec.begin(); vCit!=vec.end(); ++vCit)
			ofs << endl << vCit->c_str();
	}
}

void SimpleConfig::addValue(const std::string& section, const std::string& value)
{
	SCMI it = configMap_.find(section);
	if (it != configMap_.end()) it->second.push_back(value);
}

// find vector holding all values for the respective section and delete value
void SimpleConfig::delValue(const std::string& section, const std::string& value)
{
	SCMI it = configMap_.find(section);
	if (it != configMap_.end())
	{
		SCVI pos = find(it->second.begin(), it->second.end(), value);
		if (pos != it->second.end()) it->second.erase(pos);
	}
}

// delete value from all sections
void SimpleConfig::delValue(const std::string& value)
{
	for (SCIT secCit=sections_.begin(); secCit!=sections_.end(); ++secCit)
		delValue(*secCit, value);
}

void SimpleConfig::flush()
{
	rewriteFile();
}