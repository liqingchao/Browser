#include "stdafx.h"
#include "BrowserManager.h"
#include <sstream>
#include "include/base/cef_bind.h"
#include "include/base/cef_logging.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

namespace Browser
{
	BrowserManager* gBrowserManager = NULL;

	BrowserManager* BrowserManager::Get() {
		DCHECK(gBrowserManager);
		return gBrowserManager;
	}

	namespace Switches {
		const char kCachePath[] = "cache-path";
		const char kRequestContextPerBrowser[] = "request-context-per-browser";
		const char kRequestContextSharedCache[] = "request-context-shared-cache";
	}

	namespace {
		class ClientRequestContextHandler : public CefRequestContextHandler
		{
		public:
			ClientRequestContextHandler() {}
			bool OnBeforePluginLoad(
				const CefString& mime_type,
				const CefString& plugin_url,
				const CefString& top_origin_url,
				CefRefPtr<CefWebPluginInfo> plugin_info,
				PluginPolicy* plugin_policy) OVERRIDE
			{
				// Always allow the PDF plugin to load.
				if (*plugin_policy != PLUGIN_POLICY_ALLOW && mime_type == "application/pdf") {
					*plugin_policy = PLUGIN_POLICY_ALLOW;
					return true;
				}
				return false;
			}
		private:
			IMPLEMENT_REFCOUNTING(ClientRequestContextHandler);
		};
	}

	namespace
	{
		const TCHAR kWndClass[] = L"Client_TempWindow";
		HWND CreateTempWindow()
		{
			HINSTANCE hInstance = ::GetModuleHandle(NULL);

			WNDCLASSEX wc = {0};
			wc.cbSize = sizeof(wc);
			wc.lpfnWndProc = DefWindowProc;
			wc.hInstance = hInstance;
			wc.lpszClassName = kWndClass;
			RegisterClassEx(&wc);

			// Create a 1x1 pixel hidden window.
			return CreateWindow(kWndClass, 0,
				WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
				0, 0, 1, 1,
				NULL, NULL, hInstance, NULL);
		}
		TempWindow* g_temp_window = NULL;
	}

	TempWindow::TempWindow() : m_hWnd(NULL) {
		DCHECK(!g_temp_window);
		g_temp_window = this;
		m_hWnd = CreateTempWindow();
		CHECK(m_hWnd);
	}

	TempWindow::~TempWindow() {
		g_temp_window = NULL;
		DCHECK(m_hWnd);
		DestroyWindow(m_hWnd);
	}

	// static
	CefWindowHandle TempWindow::GetWindowHandle()
	{
		DCHECK(g_temp_window);
		return g_temp_window->m_hWnd;
	}

	BrowserManager::BrowserManager(bool terminate_when_all_windows_closed)
		: m_bAllWindowsClosed(terminate_when_all_windows_closed)
	{
		CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
		DCHECK(!gBrowserManager);
		gBrowserManager = this;
		DCHECK(command_line.get());
		m_bRequestContextPerBrowser = command_line->HasSwitch(Switches::kRequestContextPerBrowser);
		m_bRequestContextSharedBrowser = command_line->HasSwitch(Switches::kRequestContextSharedCache);

		sHomepage = _T("https://gate.io/");

	}

	BrowserManager::~BrowserManager() {
		// All root windows should already have been destroyed.
		DCHECK(m_BrowserWindowSet.empty());
	}

	std::string BrowserManager::GetDownloadPath(const std::string& file_name) {
		TCHAR szFolderPath[MAX_PATH];
		std::string path;

		// Save the file in the user's "My Documents" folder.
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE,
			NULL, 0, szFolderPath))) {
				path = CefString(szFolderPath);
				path += "\\" + file_name;
		}

		return path;
	}

	scoped_refptr<BrowserDlg> BrowserManager::CreateRootWindow()
	{
		scoped_refptr<BrowserDlg> pDlg = new Browser::BrowserDlg();
		if(pDlg){
			pDlg->Init(this, GetHomepage());

			// Store a reference to the root window on the main thread.
			OnRootWindowCreated(pDlg);
		}

		return pDlg;
	}

	scoped_refptr<BrowserDlg> BrowserManager::CreateRootWindowAsPopup(
		bool with_controls,
		const CefString& target_url,
		const CefPopupFeatures& popupFeatures,
		CefWindowInfo& windowInfo,
		CefRefPtr<CefClient>& client,
		CefBrowserSettings& settings) {
			scoped_refptr<BrowserDlg> pDlg = new Browser::BrowserDlg();
			if(pDlg){
				pDlg->InitAsPopup(this, with_controls, target_url, popupFeatures, windowInfo, client, settings);

				// Store a reference to the root window on the main thread.
				OnRootWindowCreated(pDlg);
			}

			return pDlg;
	}

	scoped_refptr<BrowserDlg> BrowserManager::GetWindowForBrowser(
		int browser_id) {
			REQUIRE_MAIN_THREAD();

			BrowserWindowSet::const_iterator it = m_BrowserWindowSet.begin();
			for (; it != m_BrowserWindowSet.end(); ++it) {
				CefRefPtr<CefBrowser> browser = (*it)->GetBrowser();
				if (browser.get() && browser->GetIdentifier() == browser_id)
					return *it;
			}
			return NULL;
	}

	void BrowserManager::CloseAllWindows(bool force) {
		if (!CURRENTLY_ON_MAIN_THREAD()) {
			// Execute this method on the main thread.
			MAIN_POST_CLOSURE(
				base::Bind(&BrowserManager::CloseAllWindows, base::Unretained(this),
				force));
			return;
		}

		if (m_BrowserWindowSet.empty())
			return;

		BrowserWindowSet::const_iterator it = m_BrowserWindowSet.begin();
		for (; it != m_BrowserWindowSet.end(); ++it)
			(*it)->Close(force);
	}

	void BrowserManager::OnRootWindowCreated(
		scoped_refptr<BrowserDlg> pDlg) {
			if (!CURRENTLY_ON_MAIN_THREAD()) {
				// Execute this method on the main thread.
				MAIN_POST_CLOSURE(
					base::Bind(&BrowserManager::OnRootWindowCreated,
					base::Unretained(this), pDlg));
				return;
			}

			m_BrowserWindowSet.insert(pDlg);
	}

	CefRefPtr<CefRequestContext> BrowserManager::GetRequestContext()
	{
		REQUIRE_MAIN_THREAD();

		if (m_bRequestContextPerBrowser) {
			// Create a new request context for each browser.
			CefRequestContextSettings settings;

			CefRefPtr<CefCommandLine> command_line =
				CefCommandLine::GetGlobalCommandLine();
			if (command_line->HasSwitch(Switches::kCachePath)) {
				if (m_bRequestContextSharedBrowser) {
					// Give each browser the same cache path. The resulting context objects
					// will share the same storage internally.
					CefString(&settings.cache_path) =
						command_line->GetSwitchValue(Switches::kCachePath);
				} else {
					// Give each browser a unique cache path. This will create completely
					// isolated context objects.
					std::stringstream ss;
					ss << command_line->GetSwitchValue(Switches::kCachePath).ToString() <<
						time(NULL);
					CefString(&settings.cache_path) = ss.str();
				}
			}

			return CefRequestContext::CreateContext(settings, new ClientRequestContextHandler);
		}

		// All browsers will share the global request context.
		if (!m_SharedRequestContext.get()) {
			m_SharedRequestContext = CefRequestContext::CreateContext(CefRequestContext::GetGlobalContext(), new ClientRequestContextHandler);
		}
		return m_SharedRequestContext;
	}

	void BrowserManager::OnExit(BrowserDlg* pDlg) {
		REQUIRE_MAIN_THREAD();

		CloseAllWindows(false);
	}

	void BrowserManager::OnRootWindowDestroyed(BrowserDlg* pDlg) {
		REQUIRE_MAIN_THREAD();

		BrowserWindowSet::iterator it = m_BrowserWindowSet.find(pDlg);
		DCHECK(it != m_BrowserWindowSet.end());
		if (it != m_BrowserWindowSet.end())
			m_BrowserWindowSet.erase(it);

		if (m_bAllWindowsClosed && m_BrowserWindowSet.empty()) {
			// Quit the main message loop after all windows have closed.
			MessageLoop::Get()->Quit();
			//PostQuitMessage(0);
		}
	}
}
