#include "mehari/CodeGen/TemplateWriter.h"

#include <fstream>


TemplateWriter::TemplateWriter() {
	dict = new ctemplate::TemplateDictionary("example");
	output.clear();
}


TemplateWriter::~TemplateWriter() {
	delete dict;
}


void TemplateWriter::setValue(std::string variable, std::string value) {
	dict->SetValue(variable, value);
}


void TemplateWriter::setValueInSection(std::string secName, std::string variable, std::string value) {
	dict->SetValueAndShowSection(variable, value, secName);
}


void TemplateWriter::enableSection(std::string secName) {
	dict->ShowSection(secName);
}


void TemplateWriter::setValueInSubTemplate(std::string tplFile, std::string tplName, std::string key, 
	std::string variable, std::string value, std::string subDictionary)  {
	ctemplate::TemplateDictionary *subDict;
	if (subDictionaries.find(key) != subDictionaries.end()) {
		// this sub-template already exists -> get it
		subDict = subDictionaries[key];
	}
	else {
		// the sub-template does not exists -> create it
		// set parent directory of the template
		ctemplate::TemplateDictionary *parentDict;
		if (!subDictionary.empty()) {
			if (subDictionaries.find(subDictionary) != subDictionaries.end())
				parentDict = subDictionaries[subDictionary];
			else
				// the given key for the sub-template was not found -> exit
				return;
		}
		else 
			parentDict = dict;
		subDict = parentDict->AddIncludeDictionary(tplName);
		subDict->SetFilename(tplFile);
		subDictionaries[key] = subDict;
	}
	subDict->SetValue(variable, value);
}


void TemplateWriter::expandTemplate(std::string tplFile) {
	ctemplate::ExpandTemplate(tplFile, ctemplate::DO_NOT_STRIP, dict, &output);
}


void TemplateWriter::writeToFile(std::string outFile) {
	std::ofstream file(outFile.c_str());
	file << output;
}
