#include "drag_drop.h"

#include <windows.h>
#include <oleidl.h>
#include <shellapi.h>
#include <shlobj.h>

#include <atomic>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace
{
HGLOBAL CreateHDropData(const std::vector<std::wstring>& filePaths)
{
    std::size_t characterCount = 1;
    for (const std::wstring& path : filePaths)
    {
        if (path.size() >
            std::numeric_limits<std::size_t>::max() - characterCount - 1)
        {
            return nullptr;
        }
        characterCount += path.size() + 1;
    }
    if (characterCount >
        (std::numeric_limits<std::size_t>::max() - sizeof(DROPFILES)) /
            sizeof(wchar_t))
    {
        return nullptr;
    }

    const std::size_t allocationSize =
        sizeof(DROPFILES) + characterCount * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,
                                 allocationSize);
    if (memory == nullptr)
    {
        return nullptr;
    }

    void* lockedMemory = GlobalLock(memory);
    if (lockedMemory == nullptr)
    {
        GlobalFree(memory);
        return nullptr;
    }

    auto* dropFiles = static_cast<DROPFILES*>(lockedMemory);
    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;

    auto* destination = reinterpret_cast<wchar_t*>(
        static_cast<unsigned char*>(lockedMemory) + sizeof(DROPFILES));
    for (const std::wstring& path : filePaths)
    {
        const std::size_t bytes = (path.size() + 1) * sizeof(wchar_t);
        std::memcpy(destination, path.c_str(), bytes);
        destination += path.size() + 1;
    }
    *destination = L'\0';
    GlobalUnlock(memory);
    return memory;
}

class FileDataObject final : public IDataObject
{
public:
    explicit FileDataObject(std::vector<std::wstring> filePaths)
        : filePaths_(std::move(filePaths))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId,
                                             void** object) override
    {
        if (object == nullptr)
        {
            return E_POINTER;
        }
        *object = nullptr;
        if (interfaceId == IID_IUnknown || interfaceId == IID_IDataObject)
        {
            *object = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++referenceCount_;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG referenceCount = --referenceCount_;
        if (referenceCount == 0)
        {
            delete this;
        }
        return referenceCount;
    }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format,
                                      STGMEDIUM* medium) override
    {
        if (format == nullptr || medium == nullptr)
        {
            return E_POINTER;
        }
        const HRESULT queryResult = QueryGetData(format);
        if (FAILED(queryResult))
        {
            return queryResult;
        }

        HGLOBAL data = CreateHDropData(filePaths_);
        if (data == nullptr)
        {
            return STG_E_MEDIUMFULL;
        }
        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = data;
        medium->pUnkForRelease = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override
    {
        return DATA_E_FORMATETC;
    }

    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* format) override
    {
        if (format == nullptr)
        {
            return E_POINTER;
        }
        if (format->cfFormat != CF_HDROP ||
            (format->tymed & TYMED_HGLOBAL) == 0 ||
            format->dwAspect != DVASPECT_CONTENT || format->lindex != -1)
        {
            return DV_E_FORMATETC;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*,
                                                     FORMATETC* output) override
    {
        if (output == nullptr)
        {
            return E_POINTER;
        }
        output->ptd = nullptr;
        return DATA_S_SAMEFORMATETC;
    }

    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction,
                                            IEnumFORMATETC** enumerator) override
    {
        if (enumerator == nullptr)
        {
            return E_POINTER;
        }
        *enumerator = nullptr;
        if (direction != DATADIR_GET)
        {
            return E_NOTIMPL;
        }

        FORMATETC format{};
        format.cfFormat = CF_HDROP;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.tymed = TYMED_HGLOBAL;
        return SHCreateStdEnumFmtEtc(1, &format, enumerator);
    }

    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*,
                                      DWORD*) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    std::atomic<ULONG> referenceCount_{1};
    std::vector<std::wstring> filePaths_;
};

class FileDropSource final : public IDropSource
{
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId,
                                             void** object) override
    {
        if (object == nullptr)
        {
            return E_POINTER;
        }
        *object = nullptr;
        if (interfaceId == IID_IUnknown || interfaceId == IID_IDropSource)
        {
            *object = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++referenceCount_;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG referenceCount = --referenceCount_;
        if (referenceCount == 0)
        {
            delete this;
        }
        return referenceCount;
    }

    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escapePressed,
                                                DWORD keyState) override
    {
        if (escapePressed)
        {
            return DRAGDROP_S_CANCEL;
        }
        if ((keyState & MK_LBUTTON) == 0)
        {
            return DRAGDROP_S_DROP;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override
    {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

private:
    std::atomic<ULONG> referenceCount_{1};
};
}

bool StartExternalFileDrag(const std::vector<std::wstring>& filePaths)
{
    if (filePaths.empty())
    {
        return false;
    }

    auto* dataObject = new (std::nothrow) FileDataObject(filePaths);
    auto* dropSource = new (std::nothrow) FileDropSource();
    if (dataObject == nullptr || dropSource == nullptr)
    {
        if (dataObject != nullptr) dataObject->Release();
        if (dropSource != nullptr) dropSource->Release();
        return false;
    }

    DWORD effect = DROPEFFECT_NONE;
    const HRESULT result = DoDragDrop(dataObject, dropSource,
                                      DROPEFFECT_COPY, &effect);
    dropSource->Release();
    dataObject->Release();
    return result == DRAGDROP_S_DROP && (effect & DROPEFFECT_COPY) != 0;
}
