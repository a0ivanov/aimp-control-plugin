//  base64.hpp
//  Autor Konstantin Pilipchuk
//  mailto:lostd@ukr.net
//
#include "stdafx.h"
#include "utils/base64.h"

namespace Base64Utils
{

int _base64Chars[64]= {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
                       'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
                       '0','1','2','3','4','5','6','7','8','9',
                       '+','/'
};

} // namespace Base64Utils
