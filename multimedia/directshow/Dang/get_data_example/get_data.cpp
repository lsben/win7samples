// From https://msdn.microsoft.com/en-us/library/ms867162.aspx "How To Get Data
// from a Microsoft DirectShow Filter Graph" by Eric Rudolph, Oct. 2003.

//#include "stdafx.h"
//#include <atlbase.h>
#include <Windows.h>

#include "qedit.h"         // for CLSID_NullRenderer; also provides
#include "streams.h"       //     ISampleGrabber but we're rolling out own.
#include "filters/grabber/grabber.h"
#include "common/utils.h"
#include "common/smartptr.h"

HANDLE gWaitEvent = NULL;  // Signaled in Callback()

// Matches CSampleGrabber::CallbackFunction.
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
    CHECK_HR_RETURN(hr);
    pSampleGrabber->AddRef();

    // Set the callback function of the filter.
    pSampleGrabber->SetCallback(&Callback);

    // Set up a partially specified media type.
    CMediaType media_type(&MEDIATYPE_Video);
    media_type.SetSubtype(&MEDIASUBTYPE_RGB24);
    hr = pSampleGrabber->SetAcceptedMediaType(&media_type);
    CHECK_HR_RETURN(hr);

    // Create the filter graph manager.
    SmartPtr<IFilterGraph> pFilterGraph;
    hr = pFilterGraph.CoCreateInstance( CLSID_FilterGraph );
    CHECK_HR_RETURN(hr);

    // Query for other useful interfaces.
    SmartPtr<IGraphBuilder> pGraphBuilder;
    CHECK_HR_RETURN(pFilterGraph.QueryInterface(&pGraphBuilder));
    SmartPtr<IMediaSeeking> pSeeking;
    CHECK_HR_RETURN(pFilterGraph.QueryInterface(&pSeeking));
    SmartPtr<IMediaControl> pControl;
    CHECK_HR_RETURN(pFilterGraph.QueryInterface(&pControl));
    SmartPtr<IMediaFilter> pMediaFilter;
    CHECK_HR_RETURN(pFilterGraph.QueryInterface(&pMediaFilter));
    SmartPtr<IMediaEvent> pEvent;
    CHECK_HR_RETURN(pFilterGraph.QueryInterface(&pEvent));
    //CComQIPtr<IMediaEvent, &IID_IMediaEvent> pEvent(pFilterGraph);

    // Add a source filter to the graph.
    SmartPtr<IBaseFilter> pSourceFilter;
    const wchar_t* kVideoFilePath =
            L"Z:\Downloads\videos\grb_2.avi";
    //      L"Z:\Downloads\videos\PredatorDroneMissileStrike.webm";
    hr = pGraphBuilder->AddSourceFilter(kVideoFilePath, L"Source",
                                        &pSourceFilter);
    CHECK_HR_RETURN(hr);

    // Add the sample grabber to the graph.
    hr = pGraphBuilder->AddFilter(pSampleGrabber, L"Grabber");
    CHECK_HR_RETURN(hr);

    // Find the input and output pins, and connect them.
    IPin *pSourceOut = GetOutPin(pSourceFilter, 0);
    IPin *pGrabIn = GetInPin(pSampleGrabber, 0);
    hr = pGraphBuilder->Connect(pSourceOut, pGrabIn);
    CHECK_HR_RETURN(hr);

    // Create the Null Renderer filter and add it to the graph.
    SmartPtr<IBaseFilter> pNull;
    hr = pNull.CoCreateInstance(CLSID_NullRenderer);
    hr = pGraphBuilder->AddFilter(pNull, L"Renderer");
    CHECK_HR_RETURN(hr);

    // Get the other input and output pins, and connect them.
    IPin *pGrabOut = GetOutPin(pSampleGrabber, 0);
    IPin *pNullIn = GetInPin(pNull, 0);
    hr = pGraphBuilder->Connect(pGrabOut, pNullIn);
    CHECK_HR_RETURN(hr);

    // Show the graph in the debug output.
    DumpGraph(pFilterGraph, 0);

    // Note: The graph is built, but we do not know the format yet.
    // To find the format, call GetConnectedMediaType. For this example,
    // we just write some information to the debug window.
    int64_t Duration = 0;
    hr = pSeeking->GetDuration(&Duration);
    CHECK_HR_RETURN(hr);
    BOOL Paused = FALSE;
    long t1 = timeGetTime();

    for(int i = 0 ; i < 100 ; i++) {
        // Seek the graph.
        REFERENCE_TIME Seek = Duration * i / 100;
        hr = pSeeking->SetPositions(&Seek, AM_SEEKING_AbsolutePositioning,
                                    NULL, AM_SEEKING_NoPositioning );
        CHECK_HR_RETURN(hr);

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
    int hr = get_data_test( argc, argv );
    CoUninitialize();
    return hr;
}
