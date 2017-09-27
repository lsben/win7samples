// From https://msdn.microsoft.com/en-us/library/ms867162.aspx "How To Get Data
// from a Microsoft DirectShow Filter Graph" by Eric Rudolph, Oct. 2003.

#include "stdafx.h"
#include <atlbase.h>
#include <streams.h>
#include <qedit.h>         // for Null Renderer
#include <filfuncs.h>      // for GetOutPin, GetInPin
#include <filfuncs.cpp>    // for GetOutPin, GetInPin
#include "SampleGrabber.h"

HANDLE gWaitEvent = NULL;

HRESULT Callback(IMediaSample* pSample, REFERENCE_TIME* StartTime,
                 REFERENCE_TIME* StopTime) {
    // Note: We cannot do anything with this sample until we call
    // GetConnectedMediaType on the filter to find out the format.
    DbgLog((LOG_TRACE, 0, "Callback with sample %lx for time %ld",
            pSample, long(*StartTime / 10000)));
    SetEvent(gWaitEvent);
    return S_FALSE; // Tell the source to stop delivering samples.
}

int get_data_test(int argc, char* argv[]) {
    // Create an event, which is signaled when we get a sample.
    gWaitEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    // The sample grabber is not in the registry, so create it with 'new'.
    HRESULT hr = S_OK;
    CSampleGrabber *pGrab = new CSampleGrabber(NULL, &hr, FALSE);
    pGrab->AddRef();

    // Set the callback function of the filter.
    pGrab->SetCallback(&Callback);

    // Set up a partially specified media type.
    CMediaType mt;
    mt.SetType(&MEDIATYPE_Video);
    mt.SetSubtype(&MEDIASUBTYPE_RGB24);
    hr = pGrab->SetAcceptedMediaType(&mt);

    // Create the filter graph manager.
    CComPtr<IFilterGraph> pGraph;
    hr = pGraph.CoCreateInstance( CLSID_FilterGraph );

    // Query for other useful interfaces.
    CComQIPtr<IGraphBuilder, &IID_IGraphBuilder> pBuilder(pGraph);
    CComQIPtr<IMediaSeeking, &IID_IMediaSeeking> pSeeking(pGraph);
    CComQIPtr<IMediaControl, &IID_IMediaControl> pControl(pGraph);
    CComQIPtr<IMediaFilter, &IID_IMediaFilter> pMediaFilter(pGraph);
    CComQIPtr<IMediaEvent, &IID_IMediaEvent> pEvent(pGraph);

    // Add a source filter to the graph.
    CComPtr<IBaseFilter> pSource;
    hr = pBuilder->AddSourceFilter(L"C:\\test.avi", L"Source", &pSource);

    // Add the sample grabber to the graph.
    hr = pBuilder->AddFilter(pGrab, L"Grabber");

    // Find the input and output pins, and connect them.
    IPin *pSourceOut = GetOutPin(pSource, 0);
    IPin *pGrabIn = GetInPin(pGrab, 0);
    hr = pBuilder->Connect(pSourceOut, pGrabIn);

    // Create the Null Renderer filter and add it to the graph.
    CComPtr<IBaseFilter> pNull;
    hr = pNull.CoCreateInstance(CLSID_NullRenderer);
    hr = pBuilder->AddFilter(pNull, L"Renderer");

    // Get the other input and output pins, and connect them.
    IPin *pGrabOut = GetOutPin(pGrab, 0);
    IPin *pNullIn = GetInPin(pNull, 0);
    hr = pBuilder->Connect(pGrabOut, pNullIn);

    // Show the graph in the debug output.
    DumpGraph(pGraph, 0);

    // Note: The graph is built, but we do not know the format yet.
    // To find the format, call GetConnectedMediaType. For this example,
    // we just write some information to the debug window.

    REFERENCE_TIME Duration = 0;
    hr = pSeeking->GetDuration(&Duration);
    BOOL Paused = FALSE;
    long t1 = timeGetTime();

    for(int i = 0 ; i < 100 ; i++) {
        // Seek the graph.
        REFERENCE_TIME Seek = Duration * i / 100;
        hr = pSeeking->SetPositions(&Seek, AM_SEEKING_AbsolutePositioning,
            NULL, AM_SEEKING_NoPositioning );

        // Pause the graph, if it is not paused yet.
        if( !Paused ) {
            hr = pControl->Pause();
            ASSERT(!FAILED(hr));
            Paused = TRUE;
        }
        // Wait for the source to deliver a sample. The callback returns
        // S_FALSE, so the source delivers one sample per seek.
        WaitForSingleObject(gWaitEvent, INFINITE);
    }

    long t2 = timeGetTime();
    DbgLog((LOG_TRACE, 0, "Frames per second = %ld", i * 1000/(t2 - t1)));
    pGrab->Release();
    return 0;
}

int main(int argc, char* argv[]) {
    CoInitialize( NULL );
    int i = get_data_test( argc, argv );
    CoUninitialize();
    return i;
}
