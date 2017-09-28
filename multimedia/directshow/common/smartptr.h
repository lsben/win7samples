// Hi-lock: (("\\_<\\(AddRef\\|Release\\|QueryInterface\\)\\_>(" (1 'underline prepend)))
//
// SmartPtr.h
//
// A [COM] smart pointer class that does not depend on any ATL headers

#pragma once

// Name: AreCOMObjectsEqual [template]
// Desc: Tests two COM pointers for equality.
template <class T1, class T2>
bool AreComObjectsEqual(T1 *p1, T2 *p2) {
    bool bResult = false;
    if (p1 == NULL && p2 == NULL) {
        // Both are NULL
        bResult = true;
    } else if (p1 == NULL || p2 == NULL) {
        // One is NULL and one is not
        bResult = false;
    } else {
        // Both are not NULL. Compare IUnknowns.
        IUnknown *pUnk1 = NULL;
        IUnknown *pUnk2 = NULL;
        if (SUCCEEDED(p1->QueryInterface(IID_IUnknown, (void**)&pUnk1))) {
            if (SUCCEEDED(p2->QueryInterface(IID_IUnknown, (void**)&pUnk2))) {
                bResult = (pUnk1 == pUnk2);
                pUnk2->Release();
            }
            pUnk1->Release();
        }
    }
    return bResult;
}

// This is a version of our COM interface that dis-allows AddRef and
// Release. All ref-counting should be done by the SmartPtr object, so we want
// to dis-allow calling AddRef or Release directly. The operator-> returns a
// _NoAddRefOrRelease pointer instead of returning the raw COM pointer. (This
// behavior is the same as ATL's CComPtr class.)
template <class T>
class _NoAddRefOrRelease : public T
{
private:
    STDMETHOD_(ULONG, AddRef)() = 0;
    STDMETHOD_(ULONG, Release)() = 0;
};

// msw:
//
// SmartPtr is similar to, but not quite drop-in replacement for,
// CComPtr. Differences: SmartPtr lacked a CoCreateInstance() method [until
// now], but it has its own QueryInterface() method [good].
//
// T is a COM type, presumably derived from IUnknown. IUnknown is
// reference-counted, so the only thing this class is responsible for is calling
// Release() and delegating QueryInterface().
template <class T> class SmartPtr
{
public:
    SmartPtr() : m_ptr(NULL) { }
    SmartPtr(const SmartPtr& sptr) {
        m_ptr = sptr.m_ptr;
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

    ~SmartPtr() {
        if (m_ptr) { m_ptr->Release(); }
    }

    SmartPtr& operator=(const SmartPtr& sptr) {
        // Don't do anything if we are assigned to ourselves.
        if (!AreComObjectsEqual(m_ptr, sptr.m_ptr)) {
            if (m_ptr) {
                m_ptr->Release();
            }
            m_ptr = sptr.m_ptr;
            if (m_ptr) {
                m_ptr->AddRef();
            }
        }
        return *this;
    }

    T** operator&() {
        return &m_ptr;
    }

    _NoAddRefOrRelease<T>* operator->() {
        return (_NoAddRefOrRelease<T>*)m_ptr;
    }

    operator T*() {
        return m_ptr;
    }

    HRESULT CoCreateInstance(REFCLSID clsid, LPUNKNOWN pUnkOuter = NULL,
                             DWORD dwClsContext = CLSCTX_INPROC_SERVER) {
        return CoCreateInstance(
                clsid, pUnkOuter, dwClsContext, __uuidof(T),
                reinterpret_cast<PVOID *>(m_ptr));
    }

    // Templated version of QueryInterface. Q is another interface type.
    template <class Q>
    HRESULT QueryInterface(Q **ppQ) {
        return m_ptr->QueryInterface(
            __uuidof(Q),
            (static_cast<IUnknown*>(*(ppQ)), reinterpret_cast<void**>(ppQ)));
        // I guess the first part in the comma expression is a sanity check;
        // only the second is actually passed on.
    }

    ULONG Release() {
        T *ptr = m_ptr;
        ULONG result = 0;
        if (ptr) {
            m_ptr = NULL;
            result = ptr->Release();
        }
        return result;
    }

	// Detach the interface (does not Release)
    T* Detach() {
        T* p = m_ptr;
        m_ptr = NULL;
        return p;
    }

    // Attach to an existing interface (does not AddRef)
    void Attach(T* p) {
		if (m_ptr) {
			m_ptr->Release();
        }
		m_ptr = p;
	}

    bool operator==(T *ptr) const
    {
        return AreComObjectsEqual(m_ptr, ptr);
    }

    bool operator!=(T *ptr) const
    {
        return !operator==(ptr);
    }

private:
    T *m_ptr;
};
