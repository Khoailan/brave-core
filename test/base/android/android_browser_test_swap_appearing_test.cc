#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"

#include <iostream>
#include <chrono>
#include <thread>

#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "brave/components/constants/brave_paths.h"
#include "base/path_service.h"
#include "brave/components/brave_search/browser/brave_search_fallback_host.h"
#include "components/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "base/strings/string_util.h"
//#include "chrome/browser/ui/browser.h"
//#include "base/files/file_path.h"
#include "components/google/core/common/google_switches.h"
#include "weblayer/browser/webui/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "base/memory/ref_counted_memory.h"
#include "content/public/common/url_constants.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/browser/webui/url_data_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "brave/components/brave_wallet/common/features.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "brave/chromium_src/chrome/test/base/testing_browser_process.h"
#include "brave/components/brave_wallet/browser/eth_allowance_manager.h"

#include "brave/browser/brave_wallet/json_rpc_service_factory.h"
#include "brave/browser/brave_wallet/keyring_service_factory.h"
#include "brave/browser/brave_wallet/tx_service_factory.h"
#include "brave/components/brave_wallet/browser/blockchain_registry.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service_delegate.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service_observer_base.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/json_rpc_service.h"
#include "brave/components/brave_wallet/browser/keyring_service.h"

namespace {
/*  
  constexpr char kManifestText[] =
    R"(<html>
<head>
<script>
const currpage = {
 content: function(){
    return document.getElementsByTagName('html')[0].innerHTML;
 }
}
</script>
</head>
<body>test</body>
</html>
)";
 constexpr char kManifestText[] =
    R"({
      "name": "Test System App",
      "display": "standalone",
      "icons": [
        {
          "src": "icon-256.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ],
      "start_url": "/pwa.html",
      "theme_color": "#00FF00"
    })";

constexpr char kPwaHtml[] =
    R"(
<html>
<head>
  <link rel="manifest" href="manifest.json">
  <style>
    body {
      background-color: white;
    }
    @media(prefers-color-scheme: dark) {
      body {
        background-color: black;
      }
    }
  </style>
  <script>
    navigator.serviceWorker.register('sw.js');
  </script>
</head>
</html>
)";

constexpr char kPage2Html[] =
    R"(
<!DOCTYPE html><title>Page 2</title>
  )";

constexpr char kSwJs[] = "globalThis.addEventListener('fetch', event => {});";
 */

/* constexpr char kMnemonic1[] =
    "divide cruise upon flag harsh carbon filter merit once advice bright "
    "drive";
constexpr char kPasswordBrave[] = "brave";
 */
class TestWebUIController : public content::WebUIController {
 public:
  explicit TestWebUIController(const std::string& source_name,
                                           content::WebUI* web_ui)
      : WebUIController(web_ui) {
    AddTestURLDataSource(source_name,
                         web_ui->GetWebContents()->GetBrowserContext());
  }
  TestWebUIController(const TestWebUIController&) =
      delete;
  TestWebUIController& operator=(
      const TestWebUIController&) = delete;
  
  private:
  void AddTestURLDataSource(const std::string& source_name,
                          content::BrowserContext* browser_context) {
                            LOG(INFO) << "!!!! AddTestURLDataSource source_name:" << source_name;

    content::URLDataManager::DeleteDataSources();
    content::WebUIDataSource* data_source =
         content::WebUIDataSource::CreateAndAdd(browser_context, source_name);


    data_source->DisableTrustedTypesCSP();
    data_source->DisableContentSecurityPolicy();
    //data_source->AddResourcePath("icon-256.png", IDR_PRODUCT_LOGO_256);
/*    data_source->SetRequestFilter(
        base::BindLambdaForTesting([](const std::string& path) {
          LOG(INFO) << "!!!! Filter1 path:" << path;
          return path == "swap";
        }),
        base::BindLambdaForTesting(
            [](const std::string& id,
              content::WebUIDataSource::GotDataCallback callback) {
                LOG(INFO) << "!!!! Filter2 id:" << id;
              scoped_refptr<base::RefCountedString> ref_contents(
                  new base::RefCountedString);
              if (id == "swap")
                ref_contents->data() = kManifestText;
              else  if (id == "pwa.html")
                ref_contents->data() = kPwaHtml;
              else if (id == "sw.js")
                ref_contents->data() = kSwJs;
              else if (id == "page2.html")
                ref_contents->data() = kPage2Html;
              else 
                NOTREACHED();

              std::move(callback).Run(ref_contents);
            }));  */
  }
};

class TestWebUIControllerFactory : public content::WebUIControllerFactory {
public:
  TestWebUIControllerFactory(const std::string& source_name) 
    : source_name_(source_name) {}

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
        LOG(INFO) << "!!!! CreateWebUIControllerForURL url:" << url << " url.host_piece():" << url.host_piece();
    if (!url.SchemeIs(content::kChromeUIScheme) ||
        url.host_piece() != source_name_) {
      return nullptr;
    }

    return std::make_unique<TestWebUIController>(source_name_,
                                                           web_ui);
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url.host_piece() ==
        history_clusters_internals::kChromeUIHistoryClustersInternalsHost)
      return history_clusters_internals::kChromeUIHistoryClustersInternalsHost;

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
  }
private:
std::string source_name_;
};

class AndroidBrowserTestSwap : public PlatformBrowserTest {
public:
  AndroidBrowserTestSwap() {
    factory_ = std::make_unique<TestWebUIControllerFactory>("wallet");
    content::WebUIControllerFactory::RegisterFactory(factory_.get());
  }

  void SetUp() override {
    PlatformBrowserTest::SetUp();

/*     scoped_feature_list_.InitAndEnableFeature(
        brave_wallet::features::kNativeBraveWalletFeature);

    TestingProfile::Builder builder;
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    builder.SetPrefService(std::move(prefs));
    profile_ = builder.Build();
    local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());
    keyring_service_ =
        brave_wallet::KeyringServiceFactory::GetServiceForContext(profile_.get());
    json_rpc_service_ =
        brave_wallet::JsonRpcServiceFactory::GetServiceForContext(profile_.get());
    json_rpc_service_->SetAPIRequestHelperForTesting(
        shared_url_loader_factory_);
    tx_service = brave_wallet::TxServiceFactory::GetServiceForContext(profile_.get());
    wallet_service_ = std::make_unique<brave_wallet::BraveWalletService>(
        shared_url_loader_factory_,
        brave_wallet::BraveWalletServiceDelegate::Create(profile_.get()), keyring_service_,
        json_rpc_service_, tx_service, GetPrefs(), GetLocalState());
    eth_allowance_manager_ = std::make_unique<brave_wallet::EthAllowanceManager>(
        json_rpc_service_, keyring_service_, GetPrefs());

    keyring_service_->RestoreWallet(kMnemonic1, kPasswordBrave, false,
                                    base::DoNothing()); */

  }

 void SetUpOnMainThread() override {
    LOG(INFO) << "SetUpOnMainThread Start";
    PlatformBrowserTest::SetUpOnMainThread();

     brave_wallet::SetDefaultEthereumWallet(
       TabModelList::models()[0]->GetProfile()->GetPrefs(),
       brave_wallet::mojom::DefaultWallet::BraveWallet);
 }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

/*   PrefService* GetPrefs() { return profile_->GetPrefs(); }
  TestingPrefServiceSimple* GetLocalState() { return local_state_->Get(); }
 */
private:
  std::unique_ptr<TestWebUIControllerFactory> factory_;
  
/*   std::unique_ptr<ScopedTestingLocalState> local_state_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<brave_wallet::KeyringService> keyring_service_ = nullptr;
  raw_ptr<brave_wallet::JsonRpcService> json_rpc_service_;
  raw_ptr<brave_wallet::TxService> tx_service;
  std::unique_ptr<brave_wallet::BraveWalletService> wallet_service_;
  std::unique_ptr<brave_wallet::EthAllowanceManager> eth_allowance_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
 */

};

IN_PROC_BROWSER_TEST_F(AndroidBrowserTestSwap, TestSwapPageAppearing1) {
  GURL url = GURL("chrome://version");
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  auto* web_contents = GetActiveWebContents();

content::WebContentsConsoleObserver console_observer(web_contents);
  web_contents->GetController().LoadURLWithParams(params);
  web_contents->GetOutermostWebContents()->Focus();
  WaitForLoadStop(web_contents);

for(auto const& msg : console_observer.messages()) {
  LOG(INFO) << "!!!MSG:" << msg.message;
}

// std::string page_html = "";
// auto result = content::EvalJs(web_contents->GetPrimaryMainFrame(), R"(document.documentElement.innerHTML)",content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1);
// page_html = result.value.DebugString();
// LOG(INFO) << "!!!Error: " << result.error;

bool is_same_url = web_contents->GetLastCommittedURL() == url;
EXPECT_TRUE(is_same_url);
if (!is_same_url) {
    DLOG(WARNING) << "Expected URL " << url << " but observed "
                  << web_contents->GetLastCommittedURL();
  }

  auto filename(base::JoinString({"mypage", std::to_string(std::rand()), ".html"},"_"));
  base::FilePath fname(FILE_PATH_LITERAL(filename.c_str()));
  base::FilePath fpath(FILE_PATH_LITERAL(""));
  web_contents->SavePage(fname, fpath, content::SavePageType::SAVE_PAGE_TYPE_AS_ONLY_HTML);

}

IN_PROC_BROWSER_TEST_F(AndroidBrowserTestSwap, TestSwapPageAppearing) {
//  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
//                                        GURL("brave://wallet/swap")));
GURL url = GURL("chrome://wallet/swap");

content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  auto* web_contents = GetActiveWebContents();

content::WebContentsConsoleObserver console_observer(web_contents);
  web_contents->GetController().LoadURLWithParams(params);
  web_contents->GetOutermostWebContents()->Focus();
  WaitForLoadStop(web_contents);

std::string page_html = "";
auto result = content::EvalJs(web_contents->GetPrimaryMainFrame(), R"(document.documentElement.innerHTML)",content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1);
page_html = result.value.DebugString();
LOG(INFO) << "!!!Error: " << result.error;
//page_html = content::ExecuteScriptAndGetValue(web_contents->GetPrimaryMainFrame(), "document.documentElement.outerHTML").DebugString();
//page_html = content::ExecuteScriptAndGetValue(web_contents->GetFocusedFrame(), "document.getElementsByTagName(\'html\')[0].innerHTML").DebugString();
//page_html = content::ExecuteScriptAndGetValue(web_contents->GetPrimaryMainFrame(), "console.log(\'ssssss\')").DebugString();
//  WaitForLoadStop(web_contents);


for(auto const& msg : console_observer.messages()) {
  LOG(INFO) << "!!!MSG:" << msg.message;
}

LOG(INFO)// << "URL:" << web_contents->GetLastCommittedURL() 
          << " page_html:" << page_html;

bool is_same_url = web_contents->GetLastCommittedURL() == url;
EXPECT_TRUE(is_same_url);
if (!is_same_url) {
    DLOG(WARNING) << "Expected URL " << url << " but observed "
                  << web_contents->GetLastCommittedURL();
  }




}
}
