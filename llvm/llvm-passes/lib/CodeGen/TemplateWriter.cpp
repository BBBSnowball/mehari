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


void TemplateWriter::setValueInSubTemplate(std::string tplFile, std::string tplName, std::string variable, std::string value) {
	ctemplate::TemplateDictionary *sub_dict;
	if (subDictionaries.find(tplName) != subDictionaries.end()) {
		// this sub-template already exists -> get it
		sub_dict = subDictionaries[tplName];
	}
	else {
		// the sub-template does not exists -> create it
		sub_dict = dict->AddIncludeDictionary(tplName);
		sub_dict->SetFilename(tplFile);
	}
	sub_dict->SetValue(variable, value);
}


void TemplateWriter::expandTemplate(std::string tplFile) {
	ctemplate::ExpandTemplate(tplFile, ctemplate::DO_NOT_STRIP, dict, &output);
}


void TemplateWriter::writeToFile(std::string outFile) {
	std::ofstream file(outFile.c_str());
	file << output;
}
