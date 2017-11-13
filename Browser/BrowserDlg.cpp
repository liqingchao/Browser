#include "stdafx.h"
#include "BrowserDlg.h"
#include "BrowserManager.h"

namespace Browser
{
	HHOOK g_hHook = NULL;

	LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode >= HC_ACTION && (wParam==VK_ESCAPE || wParam==VK_F11))
		{
			
			HWND hWnd = BrowserManager::Get()->GetMainHWND();
			if(hWnd){
				PostMessage(hWnd,WM_KEYDOWN,wParam,0);
				return TRUE;
			}
		}
		return CallNextHookEx(g_hHook,nCode,wParam,lParam);
	}

	BrowserDlg::BrowserDlg()
		: m_pBrowser(NULL),
		m_Delegate(NULL),
		m_rcBrowser(),
		m_bWithOsr(false),
		m_bIsPopup(false),
		m_bInitialized(false),
		m_bWindowDestroyed(false),
		m_bBrowserDestroyed(false),
		m_bAskingExit(false)
	{
	}

	BrowserDlg::~BrowserDlg()
	{
		DCHECK(m_bWindowDestroyed);
		DCHECK(m_bBrowserDestroyed);
		//PostQuitMessage(0);
	}

	LPCTSTR BrowserDlg::GetWindowClassName() const
	{
		return _T("BrowserDlg");
	}

	void BrowserDlg::InitWindow()
	{
		SetIcon(IDR_MAINFRAME);
	}

	void BrowserDlg::OnFinalMessage(HWND hWnd)
	{
		WindowImplBase::OnFinalMessage(hWnd);
		//delete this;
		m_bWindowDestroyed = true;
		NotifyDestroyedIfDone();
	}

	DuiLib::CDuiString BrowserDlg::GetSkinFile()
	{
		return _T("BrowserDlg.xml");
	}

	LRESULT BrowserDlg::ResponseDefaultKeyEvent(WPARAM wParam)
	{
		if (wParam == VK_RETURN)
		{
			return FALSE;
		}
		else if (wParam == VK_ESCAPE)
		{
			return TRUE;
		}
		return FALSE;
	}

	DuiLib::CControlUI* BrowserDlg::CreateControl(LPCTSTR pstrClass)
	{
		DuiLib::CControlUI* pUI = NULL;
		if (_tcsicmp(pstrClass, _T("BrowserUI")) == 0)
		{
			pUI = m_pBrowser = new Browser::BrowserUI(this);
		}
		else if (_tcsicmp(pstrClass, _T("Title")) == 0)
		{
			pUI = new Browser::TitleUI();
		}
			
		return pUI;
	}

	void BrowserDlg::Notify(DuiLib::TNotifyUI& msg)
	{
		DuiLib::CDuiString sCtrlName = msg.pSender->GetName();

		if (_tcsicmp(msg.sType,DUI_MSGTYPE_WINDOWINIT) == 0)
		{
		}
		return WindowImplBase::Notify(msg);
	}

	LRESULT BrowserDlg::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg) {
		case WM_KEYDOWN:
			switch (wParam)
			{
			case VK_ESCAPE:
				{
					if(m_bAskingExit)return 0;
					m_bAskingExit = true;
					int result = MessageBox(GetHWND(),TEXT("是否退出应用！"),TEXT("提示信息"),MB_ICONINFORMATION|MB_YESNO);
					if(result == IDYES){
						Close();
					}else{
						m_bAskingExit = false;
					}
				}
				return 0;
			default:
				break;
			}
			break;
		default:
			break;
		}
		return WindowImplBase::HandleMessage(uMsg, wParam, lParam);
	}
	void BrowserDlg::OnBrowserCreated(CefRefPtr<CefBrowser> browser)
	{
		REQUIRE_MAIN_THREAD();

		m_Browser = browser;

		if (m_bIsPopup) {
			CreateBrowserWindow(CefBrowserSettings());
		} else {
			g_hHook=SetWindowsHookEx(WH_KEYBOARD,KeyboardProc,0,GetCurrentThreadId());
		}
	}

	void BrowserDlg::OnBrowserWindowDestroyed()
	{
		REQUIRE_MAIN_THREAD();

		if (m_Browser) {
			m_Browser = NULL;
		}

		m_BrowserCtrl.reset();

		if (!m_bWindowDestroyed) {
			Close();
		}

		m_bBrowserDestroyed = true;
		NotifyDestroyedIfDone();
	}

	void BrowserDlg::OnSetAddress(const std::wstring& url)
	{
	}

	void BrowserDlg::OnSetTitle(const std::wstring& title)
	{
		SetWindowText(m_hWnd, title.c_str());
	}

	void BrowserDlg::OnSetFullscreen(bool fullscreen)
	{
	}

	void BrowserDlg::OnSetLoadingState(bool isLoading,bool canGoBack,bool canGoForward)
	{
	}

	void BrowserDlg::OnSetDraggableRegions(const std::vector<CefDraggableRegion>& regions)
	{
		REQUIRE_MAIN_THREAD();

		/*
		// Reset draggable region.
		::SetRectRgn(draggable_region_, 0, 0, 0, 0);

		// Determine new draggable region.
		std::vector<CefDraggableRegion>::const_iterator it = regions.begin();
		for (;it != regions.end(); ++it) {
		HRGN region = ::CreateRectRgn(
		it->bounds.x, it->bounds.y,
		it->bounds.x + it->bounds.width,
		it->bounds.y + it->bounds.height);
		::CombineRgn(
		draggable_region_, draggable_region_, region,
		it->draggable ? RGN_OR : RGN_DIFF);
		::DeleteObject(region);
		}

		// Subclass child window procedures in order to do hit-testing.
		// This will be a no-op, if it is already subclassed.
		if (m_hWnd) {
		WNDENUMPROC proc = !regions.empty() ? SubclassWindowsProc : UnSubclassWindowsProc;
		::EnumChildWindows(
		m_hWnd, proc, reinterpret_cast<LPARAM>(draggable_region_));
		}
		*/
	}

	void BrowserDlg::NotifyDestroyedIfDone() {
		// Notify once both the window and the browser have been destroyed.
		if (m_bWindowDestroyed && m_bBrowserDestroyed)
			m_Delegate->OnRootWindowDestroyed(this);
	}

	void BrowserDlg::Init(
		BrowserDlg::Delegate* delegate,
		bool with_controls,
		bool with_osr,
		const CefRect& bounds,
		const CefBrowserSettings& settings,
		const std::wstring& url)
	{
		DCHECK(delegate);
		DCHECK(!m_bInitialized);

		m_Delegate = delegate;
		m_bWithOsr = with_osr;

		m_rcBrowser.left = bounds.x;
		m_rcBrowser.top = bounds.y;
		m_rcBrowser.right = bounds.x + bounds.width;
		m_rcBrowser.bottom = bounds.y + bounds.height;

		CreateBrowserCtrl(url);

		m_bInitialized = true;

		// Create the native root window on the main thread.
		if (CURRENTLY_ON_MAIN_THREAD()) {
			CreateBrowserWindow(settings);
		} else {
			MAIN_POST_CLOSURE(base::Bind(&BrowserDlg::CreateBrowserWindow, this, settings));
		}
	}

	void BrowserDlg::InitAsPopup(
		BrowserDlg::Delegate* delegate,
		bool with_controls,
		bool with_osr,
		const CefPopupFeatures& popupFeatures,
		CefWindowInfo& windowInfo,
		CefRefPtr<CefClient>& client,
		CefBrowserSettings& settings)
	{
		DCHECK(delegate);
		DCHECK(!m_bInitialized);

		m_Delegate = delegate;
		m_bIsPopup = true;

		if (popupFeatures.xSet)      m_rcBrowser.left = popupFeatures.x;
		if (popupFeatures.ySet)      m_rcBrowser.top = popupFeatures.y;
		if (popupFeatures.widthSet)  m_rcBrowser.right = m_rcBrowser.left + popupFeatures.width;
		if (popupFeatures.heightSet) m_rcBrowser.bottom = m_rcBrowser.top + popupFeatures.height;

		CreateBrowserCtrl(std::wstring());

		m_bInitialized = true;

		// The new popup is initially parented to a temporary window. The native root
		// window will be created after the browser is created and the popup window
		// will be re-parented to it at that time.
		m_BrowserCtrl->GetPopupConfig(TempWindow::GetWindowHandle(),windowInfo, client, settings);
	}

	CefRefPtr<CefBrowser> BrowserDlg::GetBrowser()
	{
		REQUIRE_MAIN_THREAD();

		if (m_BrowserCtrl)
			return m_BrowserCtrl->GetBrowser();
		return NULL;
	}

	void BrowserDlg::LoadURL(const CefString& url)
	{
		if(m_Browser){
			CefRefPtr<CefFrame> mainfram = m_Browser->GetMainFrame();
			if (mainfram){
				mainfram->LoadURL(url);
			}
		}
	}

	void BrowserDlg::CreateBrowserCtrl(const std::wstring& startup_url)
	{
		if (m_bWithOsr) {
			//OsrRenderer::Settings settings;
			//MainContext::Get()->PopulateOsrSettings(&settings);
			//m_BrowserCtrl.reset(new BrowserWindowOsrWin(this, startup_url, settings));
		} else {
			m_BrowserCtrl.reset(new BrowserCtrl(this, startup_url));
		}
	}

	void BrowserDlg::CreateBrowserWindow(const CefBrowserSettings& settings) {
		REQUIRE_MAIN_THREAD();

		int x, y, width, height;
		
		RECT rcWindow = m_rcBrowser;
		Create(NULL,_T("Browser"),UI_WNDSTYLE_FRAME,WS_EX_APPWINDOW,0,0,0,0,NULL);
		if (::IsRectEmpty(&rcWindow)) {
			CenterWindow();
		} else {
			x = rcWindow.left;
			y = rcWindow.top;
			width = rcWindow.right - rcWindow.left;
			height = rcWindow.bottom - rcWindow.top;
			SetWindowPos(m_hWnd, NULL, x, y,width, height,SWP_NOZORDER);
			
			if(x == 0 && y == 0 && width == GetSystemMetrics(SM_CXSCREEN && height ==  GetSystemMetrics(SM_CYSCREEN))){
				m_Manager.SetMinInfo(width, height);
				::SetWindowPos(GetHWND(),HWND_TOPMOST,x, y, width, height, SWP_FRAMECHANGED);
			}
		}

		RECT rcBrowser;
		GetClientRect(m_hWnd, &rcBrowser);

		::SetMenu(m_hWnd, NULL);

		if (m_bIsPopup) {
			m_BrowserCtrl->ShowPopup(m_hWnd, rcBrowser.left, rcBrowser.top, rcBrowser.right - rcBrowser.left, rcBrowser.bottom - rcBrowser.top);
		} else {
			// Create the browser window.
			CefRect cef_rect(rcBrowser.left, rcBrowser.top, rcBrowser.right - rcBrowser.left, rcBrowser.bottom - rcBrowser.top);
			m_BrowserCtrl->CreateBrowser(m_hWnd, cef_rect, settings, m_Delegate->GetRequestContext(this));
			BrowserManager::Get()->SetMainHWND(m_hWnd);
		}

		//Show(ShowNormal);
	}
}
