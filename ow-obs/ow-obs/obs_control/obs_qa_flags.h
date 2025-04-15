#pragma once

//-----------------------------------------------------------------------------

#include "string"

//-----------------------------------------------------------------------------

class OBSQAFlags
{
public:

  static OBSQAFlags* Instance();
    
private:

    OBSQAFlags();
    ~OBSQAFlags();

public:

  bool GetOverrideCodeFromRegistry(std::string& code);

};
