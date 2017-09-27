// From https://msdn.microsoft.com/en-us/library/ms867162.aspx "How To Get Data
// from a Microsoft DirectShow Filter Graph" by Eric Rudolph, Oct. 2003.

#include "stdafx.h"
#include <atlbase.h>
#include <streams.h>
#include <qedit.h>         // for Null Renderer
#include <filfuncs.h>      // for GetOutPin, GetInPin
#include <filfuncs.cpp>    // for GetOutPin, GetInPin
#include "filters/grabber/grabber.h"

HANDLE gWaitEvent = NULL;  // Signaled in Callback()

// Matches SampleGrabber_Callback.
HRESULT Callback(IMediaSample* pSample, REFERENCE_TIME* StartTime,
                 REFERENCE_TIME* StopTime, bool typeChanged_ignore) {
    // Note: We cannot do anything with this sample until we call
    // GetConnectedMediaType on the filter to find out the format.
    DbgLog((LOG_TRACE, 0, "Callback with sample %lx for time %ld",
            pSample, long(*StartTime / 10000)));
    SetEvent(gWaitEvent);
    return S_FALSE; // Tell the source to stop delivering samples.
}

int get_data_test(int argc, char* argv[]) {
    gWaitEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    // The sample grabber is not in the registry, so create it with 'new'.
    HRESULT hr = S_OK;
    CSampleGrabber *pSampleGrabber = new CSampleGrabber(NULL, &hr, FALSE);
    pSampleGrabber->AddRef();

    // Set the callback function of the filter.
    pSampleGrabber->SetCallback(&Callback);

    // Set up a partially specified media type.
    CMediaType media_type;
    media_type.SetType(&MEDIATYPE_Video);
    media_type.SetSubtype(&MEDIASUBTYPE_RGB24);
    hr = pSampleGrabber->SetAcceptedMediaType(&media_type);

    // Create the filter graph manager.
    CComPtr<IFilterGraph> pFilterGraph;
    hr = pFilterGraph.CoCreateInstance( CLSID_FilterGraph );

    // Query for other useful interfaces.
    CComQIPtr<IGraphBuilder, &IID_IGraphBuilder> pGraphBuilder(pFilterGraph);
    CComQIPtr<IMediaSeeking, &IID_IMediaSeeking> pSeeking(pFilterGraph);
    CComQIPtr<IMediaControl, &IID_IMediaControl> pControl(pFilterGraph);
    CComQIPtr<IMediaFilter, &IID_IMediaFilter> pMediaFilter(pFilterGraph);
    CComQIPtr<IMediaEvent, &IID_IMediaEvent> pEvent(pFilterGraph);

    // Add a source filter to the graph.
    CComPtr<IBaseFilter> pSourceFilter;
    hr = pGraphBuilder->AddSourceFilter(L"C:\\test.avi", L"Source", &pSourceFilter);

    // Add the sample grabber to the graph.
    hr = pGraphBuilder->AddFilter(pSampleGrabber, L"Grabber");

    // Find the input and output pins, and connect them.
    IPin *pSourceOut = GetOutPin(pSourceFilter, 0);
    IPin *pGrabIn = GetInPin(pSampleGrabber, 0);
    hr = pGraphBuilder->Connect(pSourceOut, pGrabIn);

    // Create the Null Renderer filter and add it to the graph.
    CComPtr<IBaseFilter> pNull;
    hr = pNull.CoCreateInstance(CLSID_NullRenderer);
    hr = pGraphBuilder->AddFilter(pNull, L"Renderer");

    // Get the other input and output pins, and connect them.
    IPin *pGrabOut = GetOutPin(pSampleGrabber, 0);
    IPin *pNullIn = GetInPin(pNull, 0);
    hr = pGraphBuilder->Connect(pGrabOut, pNullIn);

    // Show the graph in the debug output.
    DumpGraph(pFilterGraph, 0);

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
    pSampleGrabber->Release();
    return 0;
}

int main(int argc, char* argv[]) {
    CoInitialize( NULL );
    int i = get_data_test( argc, argv );
    CoUninitialize();
    return i;
}
