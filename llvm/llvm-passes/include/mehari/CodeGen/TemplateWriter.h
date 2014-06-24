#ifndef TEMPLATE_WRITER_H
#define TEMPLATE_WRITER_H

#include <string>
#include <map>

#include <ctemplate/template.h>

class TemplateWriter {

public:
  TemplateWriter();
  ~TemplateWriter();

  void setValue(std::string variable, std::string value);
  void setValueInSection(std::string secName, std::string variable, std::string value);
  void enableSection(std::string secName);
  void setValueInSubTemplate(std::string tplFile, std::string tplName, std::string key, 
    std::string variable, std::string value, std::string subDictionary = "");

  void expandTemplate(std::string tplFile);
  void writeToFile(std::string outFile);
  static void writeToFile(const std::string& outFile, const std::string& contents);

private:
  ctemplate::TemplateDictionary *dict;
  std::map< std::string, ctemplate::TemplateDictionary*> subDictionaries;
  std::string output;
};

#endif /*TEMPLATE_WRITER_H*/
