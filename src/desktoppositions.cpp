#include "desktoppositions.h"

#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <exdisp.h>
#include <shlwapi.h>
#include <objbase.h>
#include <oleauto.h>
#endif

namespace {

#ifdef Q_OS_WIN

template <class T>
struct ComPtr {
    T *p = nullptr;
    ~ComPtr() { if (p) p->Release(); }
    T **operator&() { return &p; }
    T *operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
    void reset(T *other) { if (p) p->Release(); p = other; }
};

class ApartmentScope
{
public:
    ApartmentScope()
    {

        const HRESULT hr = CoInitializeEx(nullptr,
                                          COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        m_owner = SUCCEEDED(hr) && hr != S_FALSE;
    }
    ~ApartmentScope()
    {
        if (m_owner)
            CoUninitialize();
    }
private:
    bool m_owner = false;
};

bool readDesktopView(QHash<QString, QPoint> &positions,
                     QHash<QString, QString> &targets,
                     QSize &cell)
{
    ComPtr<IShellWindows> windows;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL,
                                IID_IShellWindows, reinterpret_cast<void **>(&windows.p))))
        return false;

    VARIANT location;
    VariantInit(&location);
    location.vt = VT_I4;
    location.lVal = CSIDL_DESKTOP;
    VARIANT empty;
    VariantInit(&empty);

    long hwnd = 0;
    ComPtr<IDispatch> dispatch;
    if (FAILED(windows->FindWindowSW(&location, &empty, SWC_DESKTOP, &hwnd,
                                     SWFO_NEEDDISPATCH, &dispatch.p)) || !dispatch)
        return false;

    ComPtr<IServiceProvider> provider;
    if (FAILED(dispatch->QueryInterface(IID_IServiceProvider,
                                        reinterpret_cast<void **>(&provider.p))) || !provider)
        return false;

    ComPtr<IShellBrowser> browser;
    if (FAILED(provider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser,
                                      reinterpret_cast<void **>(&browser.p))) || !browser)
        return false;

    ComPtr<IShellView> view;
    if (FAILED(browser->QueryActiveShellView(&view.p)) || !view)
        return false;

    ComPtr<IFolderView> folderView;
    if (FAILED(view->QueryInterface(IID_IFolderView,
                                    reinterpret_cast<void **>(&folderView.p))) || !folderView)
        return false;

    POINT spacing{0, 0};
    if (SUCCEEDED(folderView->GetSpacing(&spacing)) && spacing.x > 0 && spacing.y > 0)
        cell = QSize(spacing.x, spacing.y);

    ComPtr<IShellFolder> folder;
    if (FAILED(folderView->GetFolder(IID_IShellFolder,
                                     reinterpret_cast<void **>(&folder.p))) || !folder)
        return false;

    ComPtr<IEnumIDList> items;
    if (FAILED(folderView->Items(SVGIO_ALLVIEW, IID_IEnumIDList,
                                 reinterpret_cast<void **>(&items.p))) || !items)
        return false;

    LPITEMIDLIST item = nullptr;
    while (items->Next(1, &item, nullptr) == S_OK) {
        POINT point{-1, -1};
        if (SUCCEEDED(folderView->GetItemPosition(item, &point))) {

            wchar_t displayName[MAX_PATH] = {0};
            STRRET strret;
            if (SUCCEEDED(folder->GetDisplayNameOf(item, SHGDN_INFOLDER, &strret)))
                StrRetToBufW(&strret, item, displayName, MAX_PATH);

            const QString name = QString::fromWCharArray(displayName);
            if (!name.isEmpty())
                positions.insert(name, QPoint(point.x, point.y));

            if (name.endsWith(QLatin1String(".lnk"), Qt::CaseInsensitive)) {
                LPWSTR target = nullptr;
                if (SUCCEEDED(folder->GetDisplayNameOf(item, SHGDN_FORPARSING, &strret))
                    && SUCCEEDED(StrRetToStrW(&strret, item, &target))) {
                    const QString full = QString::fromWCharArray(target);
                    if (!full.isEmpty())
                        targets.insert(name, full);
                    CoTaskMemFree(target);
                }
            }
        }
        CoTaskMemFree(item);
        item = nullptr;
    }
    return true;
}

#endif

}

namespace DesktopPositions {

Geometry fallbackGeometry()
{
    Geometry geometry;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        geometry.screen = screen->availableGeometry().size();
        const qreal ratio = screen->devicePixelRatio();

        geometry.screen = QSize(qRound(geometry.screen.width() * ratio),
                                qRound(geometry.screen.height() * ratio));
    }
    if (geometry.screen.isEmpty())
        geometry.screen = QSize(1920, 1080);

    geometry.cell = QSize(75, 98);
    geometry.live = false;
    return geometry;
}

QString resolveShortcut(const QString &path)
{
#ifdef Q_OS_WIN
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, reinterpret_cast<void **>(&link))))
        return {};

    QString resolved;
    IPersistFile *file = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile,
                                       reinterpret_cast<void **>(&file))) && file) {
        if (SUCCEEDED(file->Load(reinterpret_cast<LPCWSTR>(path.utf16()), STGM_READ))) {
            wchar_t buffer[MAX_PATH] = {0};
            if (SUCCEEDED(link->GetPath(buffer, MAX_PATH, nullptr, 0)) && buffer[0])
                resolved = QString::fromWCharArray(buffer);
        }
        file->Release();
    }
    link->Release();
    return resolved;
#else
    Q_UNUSED(path);
    return {};
#endif
}

QHash<QString, QPoint> read(QHash<QString, QString> *targets, Geometry *geometry)
{
    QHash<QString, QPoint> positions;
    Geometry local = fallbackGeometry();

#ifdef Q_OS_WIN
    ApartmentScope apartment;
    QHash<QString, QString> resolved;
    QSize cell;
    const bool live = readDesktopView(positions, resolved, cell);
    if (live) {
        local.live = true;
        if (cell.isValid())
            local.cell = cell;
        if (targets)
            *targets = resolved;
    }
#else
    Q_UNUSED(targets);
#endif

    if (geometry)
        *geometry = local;
    return positions;
}

}
