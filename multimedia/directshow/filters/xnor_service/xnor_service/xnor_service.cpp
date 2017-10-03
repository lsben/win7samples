// Author: Matthew Weaver <matthew@xnor.ai>
//

const AMOVIESETUP_MEDIATYPE sudPinTypes = {
    &MEDIATYPE_Video,
    &MEDIASUBTYPE_RGB24
};

const AMOVIESETUP_PIN psudPins[] = {  // REGFILTERPINS
    { L"Input",           // strName
      FALSE,              // bRendered
      FALSE,              // bOutput
      FALSE,              // bZero
      FALSE,              // bMany
      &CLSID_NULL,        // clsConnectsToFilter
      L"Output",          // strConnectsToPin
      1,                  // nMediaTypes
      &sudPinTypes },     // pMediaTypes
    { L"Output",
      FALSE,
      TRUE,
      FALSE,
      FALSE,
      &CLSID_NULL,
      L"Input",
      1,
      &sudPinTypes }
};

const AMOVIESETUP_FILTER sudXnorService = {
    &CLSID_XnorService,   // class id
    L"XnorService",       // strName
    MERIT_DO_NOT_USE,     // dwMerit
    2,                    // nPins
    psudPins              // pPin
};

// Needed for the CreateInstance mechanism
CFactoryTemplate g_Templates[]= {
    { L"XnorService",
      &CLSID_XnorService,
      XnorService::CreateInstance,
      NULL,
      &sudXnorService }
};

int g_nTemplates = 1;

// Exported entry points for registration and unregistration.
STDAPI DllRegisterServer() {
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer() {
    return AMovieDllRegisterServer2(FALSE);
}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  dwReason,
                      LPVOID lpReserved) {
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

