#include "Display.h"

Display::Display(const char* name):
    GenericSLM(name)
{ 
    return;
}

int Display::StoreImage(unsigned char* pixels) 
{
    return 1;
}

Display::~Display()
{
    Shutdown();
}